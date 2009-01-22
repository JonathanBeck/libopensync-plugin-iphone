/*
  Copyright (c) 2008 Instituto Nokia de Tecnologia
  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification,
  are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.
  * Neither the name of the INdT nor the names of its contributors
  may be used to endorse or promote products derived from this software
  without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/
/**
 * @file   xslt_aux.h
 * @author Adenilson Cavalcanti da Silva <adenilson.silva@indt.org.br>
 * @date   Mon Aug 25 15:50:20 2008
 *
 * @brief  A XSLT helper module, converts a XML doc to another format
 * using a xslt file.
 *
 * Depends on libxml and libxslt.
 *
 * \todo:
 * - doxygen comments about use
 * - move code to a '.c' file or make functions static
 * - make 'xslt_resources' an abstract type
 *
 */

#ifndef __XSLT_AUX__
#define __XSLT_AUX__

#include <libxml/HTMLtree.h>
#include <libxml/catalog.h>
#include <libxml/debugXML.h>
#include <libxml/xinclude.h>
#include <libxml/xmlIO.h>
#include <libxml/xmlmemory.h>
#include <libxslt/transform.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/xsltutils.h>
#include <string.h>

struct xslt_resources {
	xmlDocPtr output;
	xmlDocPtr doc;
	xsltStylesheetPtr cur;
	xmlChar *xml_str;
	int length;
	char init_flag;
};

struct xslt_resources *xslt_new(void)
{
	struct xslt_resources *result;
	result = (struct xslt_resources *)malloc(sizeof(struct xslt_resources));
	if (result)
		memset(result, 0, sizeof(struct xslt_resources));

	return result;
}

int xslt_initialize(struct xslt_resources *ctx, const char *stylesheet_path)
{
	int result = -1;
	if (!stylesheet_path || !ctx)
		goto exit;

	xmlSubstituteEntitiesDefault(1);
	xmlLoadExtDtdDefaultValue = 1;

	if (ctx->cur)
		xsltFreeStylesheet(ctx->cur);

	ctx->cur = xsltParseStylesheetFile((const xmlChar *)stylesheet_path);
	if (!ctx->cur) {
		fprintf(stderr, "Cannot create XSLT context!\n");
		goto exit;
	}

	result = 0;
	ctx->init_flag = 1;
exit:
	return result;
}

int xslt_transform(struct xslt_resources *ctx, const char *document)
{
	int result = -1;
	if (!ctx || !document)
		goto exit;

	if (ctx->doc)
		xmlFreeDoc(ctx->doc);
	if (ctx->output)
		xmlFreeDoc(ctx->output);
	ctx->output = NULL;

	ctx->doc = xmlReadMemory(document, strlen(document), "noname.xml",
				 NULL, 0);
	if (!ctx->doc) {
		fprintf(stderr, "Cannot create document with "
			"entry!\n");
		goto cleanup;
	}

	ctx->output = xsltApplyStylesheet(ctx->cur, ctx->doc, NULL);
	if (!ctx->output) {
		fprintf(stderr, "Cannot create document with "
			"output!\n");
		goto cleanup;
	}

	if (ctx->xml_str) {
		xmlFree(ctx->xml_str);
		ctx->xml_str = NULL;
	}
	xmlDocDumpMemory(ctx->output, &(ctx->xml_str), &(ctx->length));

	result = 0;

cleanup:
	if (ctx->doc)
		xmlFreeDoc(ctx->doc);
	ctx->doc = NULL;

	if (ctx->output)
		xmlFreeDoc(ctx->output);
	ctx->output = NULL;
exit:

	return result;
}

void xslt_delete(struct xslt_resources *ctx)
{
	if (!ctx)
		return;

	/* TODO: 'output' is pointing to a non null value (probably an
	 * overflow in some other place) even when not initialized.
	 * It happens just after all clients are disconnected.
	 * I must investigate it further, for while the flag is a work
	 * around to stop msync to crash.
	 */
/* 	fprintf(stderr, "\ndoc: %x\tout: %x\txml: %x\tcur: %x\n\n", */
/* 		ctx->doc, ctx->output, ctx->xml_str, ctx->cur); */

	if (!ctx->init_flag)
		goto exit;


	if (ctx->doc)
		xmlFreeDoc(ctx->doc);
	if (ctx->output)
		xmlFreeDoc(ctx->output);
	if (ctx->xml_str)
		xmlFree(ctx->xml_str);
	if (ctx->cur) {
		xsltFreeStylesheet(ctx->cur);
		xsltCleanupGlobals();
		xmlCleanupParser();
	}

exit:

	free(ctx);
}

#endif
