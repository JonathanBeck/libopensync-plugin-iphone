/** iPhone plugin
 *
 * Copyright (c) 2009 Jonathan Beck <jonabeck@gmail.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Lesser Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 */

#include <opensync/opensync.h>
#include <opensync/opensync-data.h>
#include <opensync/opensync-format.h>
#include <opensync/opensync-xmlformat.h>
#include <opensync/opensync-plugin.h>
#include <opensync/opensync-context.h>
#include <opensync/opensync-helper.h>
#include <opensync/opensync-version.h>
#include <opensync/opensync-time.h>

#include <string.h>
#include <time.h>

#include <libiphone/libiphone.h>
#include <plist/plist.h>

#include <libxml/tree.h>
#include <libxml/xpath.h>

#include "xslt_aux.h"

typedef struct iphone_env {
	/* device and service link */
	iphone_device_t device;
	iphone_lckd_client_t lckd;
	iphone_msync_client_t msync;
	/* calendar sink/format */
	OSyncObjTypeSink *calendar_sink;
	OSyncObjFormat *calendar_format;
	/* contact sink/format */
	OSyncObjTypeSink *contact_sink;
	OSyncObjFormat *contact_format;
	/* xslt helper*/
	char *xslt_path;
	struct xslt_resources *xslt_ctx_pcal;
	struct xslt_resources *xslt_ctx_pcont;
} iphone_env;

typedef enum {
	SLOW_SYNC,
	FAST_SYNC,
} session_type;

static void free_env(iphone_env *env)
{
	if (env) {
		if (env->contact_sink)
			osync_objtype_sink_unref(env->contact_sink);
		if (env->contact_format)
			osync_objformat_unref(env->contact_format);
		if (env->calendar_sink)
			osync_objtype_sink_unref(env->calendar_sink);
		if (env->calendar_format)
			osync_objformat_unref(env->calendar_format);
		if (env->xslt_path)
			free(env->xslt_path);
		if (env->xslt_ctx_pcal)
			xslt_delete(env->xslt_ctx_pcal);
		if (env->xslt_ctx_pcont)
			xslt_delete(env->xslt_ctx_pcont);

		osync_free(env);
	}
}

static void connect(void *userdata, OSyncPluginInfo *info, OSyncContext *ctx)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p, %p)", __func__, userdata, info, ctx);
	iphone_env *env = (iphone_env *)userdata;

	/*
	 * Now connect to iphone
	 */
	if (env->device || env->lckd || env->msync)
		goto already_connected; //service already started

	int res = 0;
	if ( IPHONE_E_SUCCESS == iphone_get_device( &(env->device) ) && env->device ) {
		if (IPHONE_E_SUCCESS == iphone_lckd_new_client( env->device, &(env->lckd)) && env->lckd ) {

			int port = 0;
			if (IPHONE_E_SUCCESS == iphone_lckd_start_service ( env->lckd, "com.apple.mobilesync", &port ) && port != 0 ) {
				if (IPHONE_E_SUCCESS == iphone_msync_new_client ( env->device, 3458, port, &(env->msync)) && env->msync )
					res = 1;
			}
		}
	}
	if (!res)
		goto error;

/*
	//you can also use the anchor system to detect a device reset
	//or some parameter change here. Check the docs to see how it works
	char *lanchor = NULL;
	//Now you get the last stored anchor from the device
	char *anchorpath = osync_strdup_printf("%s/anchor.db", osync_plugin_info_get_configdir(info));

	if (!osync_anchor_compare(anchorpath, "lanchor", lanchor))
		osync_objtype_sink_set_slowsync(sink, TRUE);

	osync_free(anchorpath);
*/
	char buffer[512];
	int result = 0;
	snprintf(buffer, sizeof(buffer) - 1, "%s/pcont2osync.xslt",
			env->xslt_path);
	if ((result = xslt_initialize(env->xslt_ctx_pcont, buffer)))
		goto error;
	osync_trace(TRACE_INTERNAL, "\ndone contact: %s\n", buffer);

	osync_context_report_success(ctx);
	osync_trace(TRACE_EXIT, "%s", __func__);
	return;

already_connected :
	osync_context_report_error(ctx, OSYNC_ERROR_LOCKED, "MobileSync client is already runing");
	return;

error:
	osync_context_report_error(ctx, OSYNC_ERROR_NO_CONNECTION, "Failed to start MobileSync service");

	iphone_msync_free_client(env->msync);
	env->msync = NULL;
	iphone_lckd_free_client(env->lckd);
	env->lckd = NULL;
	iphone_free_device(env->device);
	env->device = NULL;
	return;
}

plist_t build_contact_hello_msg(iphone_env *env)
{
	plist_t array = NULL;

	array = plist_new_array();
	plist_add_sub_string_el(array, "SDMessageSyncDataClassWithDevice");
	plist_add_sub_string_el(array, "com.apple.Contacts");

	//get last anchor and send new one
	OSyncError *anchor_error;
	char *timestamp = NULL;
	timestamp = osync_anchor_retrieve(osync_objtype_sink_get_anchor(env->contact_sink),
					  &anchor_error);

	if (timestamp && strlen(timestamp) > 0)
		osync_trace(TRACE_INTERNAL, "timestamp is: %s\n", timestamp);
	else {
		if (timestamp)
			free(timestamp);
		timestamp = strdup("---");
		osync_trace(TRACE_INTERNAL, "first sync!\n");
	};

	time_t t = time(NULL);

	char* new_timestamp = osync_time_unix2vtime(&t);

	plist_add_sub_string_el(array, timestamp);
	plist_add_sub_string_el(array, new_timestamp);

	plist_add_sub_uint_el(array, 106);
	plist_add_sub_string_el(array, "___EmptyParameterString___");

	return array;
}

void get_session_type_and_timestamp (plist_t array, char* sink, char** old_timestamp, char** new_timestamp, session_type* sync)
{
	if (array) {
		plist_t sink_node = plist_find_node_by_string(array, sink);
		plist_t old_ts = plist_get_next_sibling(sink_node);
		plist_t new_ts = plist_get_next_sibling(old_ts);
		plist_t type = plist_get_next_sibling(new_ts);
		plist_t snum = plist_get_next_sibling(type);

		uint64_t snumber = 0;
		char* s_type = NULL;
		plist_get_string_val(old_ts, old_timestamp);
		plist_get_string_val(new_ts, new_timestamp);
		plist_get_string_val(type, &s_type);
		plist_get_uint_val(snum, &snumber);

		if (!strcmp(s_type, "SDSyncTypeFast"))
			*sync = FAST_SYNC;
		else
			*sync = SLOW_SYNC;

	}
}

static void process_plist_new_contact(iphone_env *env, plist_t contacts, session_type type, OSyncPluginInfo *info, OSyncContext *ctx)
{
	char slow_sync_flag = 0;
	OSyncError *error = NULL;
	OSyncXMLFormat *xmlformat;
	OSyncData *odata = NULL;
	OSyncChange *chg = NULL;
	char *plist_xml = NULL;
	uint32_t length = 0;
	int result = 0;
	xmlDocPtr contact_doc = NULL;
	xmlXPathContext *xpath_ctx;
	xmlXPathObject *xpathObj;
	xmlNode *uid_node;
	char *uid = NULL;

	plist_to_xml(contacts, &plist_xml, &length);
	if ((result = xslt_transform(env->xslt_ctx_pcont, plist_xml)))
		goto error;

	free(plist_xml);
	plist_xml = env->xslt_ctx_pcont->xml_str;

	//now loop over contacts

	xmlDocPtr plist_doc = xmlReadMemory(plist_xml, length, NULL, NULL, 0);
	if (!plist_doc)
		goto error;
	xmlNodePtr root_node = xmlDocGetRootElement(plist_doc);
	if (!root_node)
		goto error;
	
	xmlNodePtr node = NULL;
	for (node = root_node->children; node; node = node->next) {

		contact_doc = xmlNewDoc("1.0");
		xmlDocSetRootElement(contact_doc, xmlCopyNode( node,1));
		
		char *contact_xml = NULL;
		int size = 0;
		xmlDocDumpMemory(contact_doc, (xmlChar **) &contact_xml, &size);

		xmlformat = osync_xmlformat_parse(contact_xml, strlen(contact_xml), &error);
		if (!xmlformat)
			goto error;
	
		osync_xmlformat_sort(xmlformat);
	
		odata = osync_data_new(xmlformat,
					osync_xmlformat_size(),
					env->contact_format, &error);
	
		if (!odata)
			goto cleanup;
	
		if (!(chg = osync_change_new(&error)))
			goto cleanup;
		osync_data_set_objtype(odata, osync_objtype_sink_get_name(env->contact_sink));
		osync_change_set_data(chg, odata);
		osync_data_unref(odata);

		//compute uid
		xpath_ctx = xmlXPathNewContext(contact_doc);
		xpathObj = xmlXPathEvalExpression("/contact/Uid/content", xpath_ctx);

		//check that there is only one field
		if ( 1 != xpathObj->nodesetval->nodeNr)
			goto cleanup;

		uid_node = xpathObj->nodesetval->nodeTab[0];

		uid = strdup((char *) xmlNodeGetContent(uid_node));
		xmlXPathFreeContext(xpath_ctx);
		xmlXPathFreeObject(xpathObj);
		xmlFreeDoc(contact_doc);

		osync_change_set_uid(chg, uid);
		free(uid);
		uid = NULL;

		if (SLOW_SYNC == type)
			osync_change_set_changetype(chg, OSYNC_CHANGE_TYPE_ADDED);
		else
//			if (gcal_contact_is_deleted(contact)) {
//				osync_change_set_changetype(chg, OSYNC_CHANGE_TYPE_DELETED);
//				osync_trace(TRACE_INTERNAL, "deleted entry!");
//			}
//			else
				osync_change_set_changetype(chg, OSYNC_CHANGE_TYPE_MODIFIED);
	
		osync_context_report_change(ctx, chg);
		osync_change_unref(chg);
		
	}
exit:
	osync_context_report_success(ctx);
	return;

cleanup:
	osync_error_unref(&error);
	osync_xmlformat_unref(&xmlformat);
	xmlXPathFreeContext(xpath_ctx);
	xmlXPathFreeObject(xpathObj);
	xmlFreeDoc(contact_doc);
	free(uid);

error:
	osync_context_report_error(ctx, OSYNC_ERROR_GENERIC, "Error processing contact plist");
}

static void slow_contact_sync(iphone_env *env, OSyncPluginInfo *info, OSyncContext *ctx)
{
	plist_t array = NULL;
	array = plist_new_array();
	plist_add_sub_string_el(array, "SDMessageGetAllRecordsFromDevice");
	plist_add_sub_string_el(array, "com.apple.Contacts");

	iphone_error_t ret = IPHONE_E_UNKNOWN_ERROR;

	ret = iphone_msync_send(env->msync, array);
	plist_free(array);
	array = NULL;

	ret = iphone_msync_recv(env->msync, &array);

	plist_t contact_node;
	plist_t switch_node;

	contact_node = plist_find_node_by_string(array, "com.apple.Contacts");
	switch_node = plist_find_node_by_string(array, "SDMessageDeviceReadyToReceiveChanges");

	//create the contacts document
	plist_t contacts = plist_new_array();

	while (NULL == switch_node) {

		//special treatment for contact ref plist
		if (plist_find_node_by_string(array, "com.apple.contacts.Contact")) {
			plist_t contact_ref_dict = plist_new_dict();
			plist_add_sub_key_el(contact_ref_dict, "contact-ref");
			plist_add_sub_node(contact_ref_dict, array);
			plist_add_sub_node(contacts, contact_ref_dict);
		}
		else
			plist_add_sub_node(contacts, array);

		array = NULL;

		array = plist_new_array();
		plist_add_sub_string_el(array, "SDMessageAcknowledgeChangesFromDevice");
		plist_add_sub_string_el(array, "com.apple.Contacts");

		ret = iphone_msync_send(env->msync, array);
		plist_free(array);
		array = NULL;

		ret = iphone_msync_recv(env->msync, &array);

		contact_node = plist_find_node_by_string(array, "com.apple.Contacts");
		switch_node = plist_find_node_by_string(array, "SDMessageDeviceReadyToReceiveChanges");
	}
	plist_free(array);
	array = NULL;

	array = plist_new_array();
	plist_add_sub_string_el(array, "DLMessagePing");
	plist_add_sub_string_el(array, "Preparing to get changes for device");

	ret = iphone_msync_send(env->msync, array);
	plist_free(array);
	array = NULL;

	array = plist_new_array();
	plist_add_sub_string_el(array, "SDMessageFinishSessionOnDevice");
	plist_add_sub_string_el(array, "com.apple.Contacts");

	ret = iphone_msync_send(env->msync, array);
	plist_free(array);
	array = NULL;

	ret = iphone_msync_recv(env->msync, &array);

	plist_t finished = plist_find_node_by_string(array, "SDMessageDeviceFinishedSession");
	contact_node = plist_find_node_by_string(array, "com.apple.Contacts");

	//now process collected informations
	process_plist_new_contact(env, contacts, SLOW_SYNC, info, ctx);

}

static void fast_contact_sync(iphone_env *env, OSyncPluginInfo *info, OSyncContext *ctx)
{
}

static void get_contact_changes(void *userdata, OSyncPluginInfo *info, OSyncContext *ctx)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p, %p)", __func__, userdata, info, ctx);

	iphone_env *env = (iphone_env *)userdata;
//	OSyncFormatEnv *formatenv = osync_plugin_info_get_format_env(info);
//	OSyncObjTypeSink *sink = osync_plugin_info_get_sink(info);


	OSyncError *error = NULL;
	iphone_error_t ret = IPHONE_E_UNKNOWN_ERROR;

	plist_t array = build_contact_hello_msg(env);
	ret = iphone_msync_send(env->msync, array);
	plist_free(array);
	array = NULL;
	ret = iphone_msync_recv(env->msync, &array);

	if ( IPHONE_E_SUCCESS == ret && array) {

		char* old_timestamp = NULL;
		char* new_timestamp = NULL;
		session_type type;
		get_session_type_and_timestamp (array, "com.apple.Contacts", &old_timestamp, &new_timestamp, &type);
		plist_free(array);
		array = NULL;

		if (SLOW_SYNC == type)
			slow_contact_sync(env, info, ctx);
		else 
			fast_contact_sync(env, info, ctx);
	}

	//Now we need to answer the call
	osync_context_report_success(ctx);
	osync_trace(TRACE_EXIT, "%s", __func__);
	return;

error :
	osync_context_report_error(ctx, OSYNC_ERROR_GENERIC, "Synchronisation failed");
	return;
}

static void commit_contact_change(void *userdata, OSyncPluginInfo *info, OSyncContext *ctx, OSyncChange *change)
{
	iphone_env *env = (iphone_env *)userdata;
	OSyncObjTypeSink *sink = osync_plugin_info_get_sink(info);
	
	/*
	 * Here you have to add, modify or delete a object
	 * 
	 */
// 	switch (osync_change_get_changetype(change)) {
// 		case OSYNC_CHANGE_TYPE_DELETED:
// 			//Delete the change
// 			//Dont forget to answer the call on error
// 			break;
// 		case OSYNC_CHANGE_TYPE_ADDED:
// 			//Add the change
// 			//Dont forget to answer the call on error
// 			osync_change_set_hash(change, "new hash");
// 			break;
// 		case OSYNC_CHANGE_TYPE_MODIFIED:
// 			//Modify the change
// 			//Dont forget to answer the call on error
// 			osync_change_set_hash(change, "new hash");
// 			break;
// 		default:
// 			;
// 	}
// 
// 	//If you are using hashtables you have to calculate the hash here:
// 	osync_hashtable_update_change(sinkenv->hashtable, change);

	//Answer the call
	osync_context_report_success(ctx);
}

static void sync_done(void *userdata, OSyncPluginInfo *info, OSyncContext *ctx)
{
	/*
	 * This function will only be called if the sync was successful
	 */
	OSyncError *error = NULL;
	OSyncObjTypeSink *sink = osync_plugin_info_get_sink(info);
	
	//If we use anchors we have to update it now.
	//Now you get/calculate the current anchor of the device
// 	char *lanchor = NULL;
// 	char *anchorpath = osync_strdup_printf("%s/anchor.db", osync_plugin_info_get_configdir(info));
// 	osync_anchor_update(anchorpath, "lanchor", lanchor);
// 	osync_free(anchorpath);
	//Save hashtable to database

	
	//Answer the call
	osync_context_report_success(ctx);
	return;
error:
	osync_context_report_osyncerror(ctx, error);
	osync_error_unref(&error);
	return;
}

static void disconnect(void *userdata, OSyncPluginInfo *info, OSyncContext *ctx)
{
	//Close all stuff you need to close
	iphone_env *env = (iphone_env *)userdata;

	iphone_msync_free_client(env->msync);
	env->msync = NULL;
	iphone_lckd_free_client(env->lckd);
	env->lckd = NULL;
	iphone_free_device(env->device);
	env->device = NULL;
	
	//Answer the call
	osync_context_report_success(ctx);
}

static void finalize(void *userdata)
{
	iphone_env *env = (iphone_env *)userdata;

	//Free all stuff that you have allocated here.
	free_env(env);
}


static void *initialize(OSyncPlugin *plugin, OSyncPluginInfo *info, OSyncError **error)
{
	/*
	 * get the config
	 */
	OSyncPluginConfig *config = osync_plugin_info_get_config(info);
	if (!config) {
		osync_error_set(error, OSYNC_ERROR_GENERIC, "Unable to get config.");
		goto error;
	}
	/*
	 * You need to specify the <some name>_environment somewhere with
	 * all the members you need
	*/
	iphone_env *env = osync_try_malloc0(sizeof(iphone_env), error);
	if (!env)
		goto error;
	memset(env, 0, sizeof(iphone_env));

	osync_trace(TRACE_INTERNAL, "The config: %s", osync_plugin_info_get_config(info));

	/* 
	 * Process the config here and set the options on your environment
	*/
	/*
	 * Process plugin specific advanced options 
	 */
	OSyncPluginAdvancedOption *advanced = osync_plugin_config_get_advancedoption_value_by_name(config, "xslt");
	if (!advanced) {
		osync_trace(TRACE_INTERNAL, "Cannot locate xslt config!\n");
		goto error_free_env;
	}

	if (!(env->xslt_path = strdup(osync_plugin_advancedoption_get_value(advanced))))
		goto error_free_env;


	//allocate contact sink
	OSyncObjTypeSinkFunctions functions_contact;
	memset(&functions_contact, 0, sizeof(OSyncObjTypeSinkFunctions));
	functions_contact.connect = connect;
	functions_contact.get_changes = get_contact_changes;
	functions_contact.commit = commit_contact_change;
	functions_contact.disconnect = disconnect;
	functions_contact.sync_done = sync_done;

	osync_trace(TRACE_INTERNAL, "\tcreating contact sink...\n");
	OSyncFormatEnv *formatenv = osync_plugin_info_get_format_env(info);
	env->contact_format = osync_format_env_find_objformat(formatenv, "xmlformat-contact");
	if (!env->contact_format) {
		osync_trace(TRACE_ERROR, "%s", "Failed to find objformat xmlformat-contact!");
		goto error_free_env;
	}
	osync_objformat_ref(env->contact_format);

	env->contact_sink = osync_plugin_info_find_objtype(info, "contact");
	if (!env->contact_sink) {
		osync_trace(TRACE_ERROR, "%s", "Failed to find objtype contact!");
		goto error_free_env;
	}

	osync_objtype_sink_set_functions(env->contact_sink, functions_contact, env);
	osync_objtype_sink_enable_anchor(env->contact_sink, TRUE);
	osync_plugin_info_add_objtype(info, env->contact_sink);

	if (!(env->xslt_ctx_pcont = xslt_new()))
		goto error_free_env;
	else
		osync_trace(TRACE_INTERNAL, "\tsucceed creating xslt_pcont!\n");

	//Now your return your struct.
	return (void *) env;

error_free_env:
	free_env(env);
error:
	osync_trace(TRACE_EXIT_ERROR, "%s: %s", __func__, osync_error_print(error));
	return NULL;
}

/* Here we actually tell opensync which sinks are available. */
static osync_bool discover(OSyncPluginInfo *info, void *userdata, OSyncError **error)
{
	osync_trace(TRACE_ENTRY, "%s(%p, %p, %p)", __func__, userdata, info, error);

//	iphone_env *env = (iphone_env *)userdata;

	// Report avaliable sinks...
	OSyncObjTypeSink *sink = osync_plugin_info_find_objtype(info, "contact");
	if (!sink) {
		return FALSE;
	}
	osync_objtype_sink_set_available(sink, TRUE);
	
	OSyncVersion *version = osync_version_new(error);
	osync_version_set_plugin(version, "iphone-sync");
	//osync_version_set_version(version, "version");
	osync_version_set_modelversion(version, "3G");
	//osync_version_set_firmwareversion(version, "firmwareversion");
	//osync_version_set_softwareversion(version, "softwareversion");
	//osync_version_set_hardwareversion(version, "hardwareversion");
	osync_plugin_info_set_version(info, version);
	osync_version_unref(version);

	osync_trace(TRACE_EXIT, "%s", __func__);
	return TRUE;
}


osync_bool get_sync_info(OSyncPluginEnv *env, OSyncError **error)
{
	//Now you can create a new plugin information and fill in the details
	//Note that you can create several plugins here
	OSyncPlugin *plugin = osync_plugin_new(error);
	if (!plugin)
		goto error;
	
	//Tell opensync something about your plugin
	osync_plugin_set_name(plugin, "iphone-sync");
	osync_plugin_set_longname(plugin, "iphone-sync plugin");
	osync_plugin_set_description(plugin, "iPhone plugin");

	//Now set the function we made earlier
	osync_plugin_set_initialize(plugin, initialize);
	osync_plugin_set_finalize(plugin, finalize);
	osync_plugin_set_discover(plugin, discover);

	osync_plugin_env_register_plugin(env, plugin);
	osync_plugin_unref(plugin);

	return TRUE;
error:
	osync_trace(TRACE_ERROR, "Unable to register: %s", osync_error_print(error));
	osync_error_unref(error);
	return FALSE;
}

int get_version(void)
{
	return 1;
}
