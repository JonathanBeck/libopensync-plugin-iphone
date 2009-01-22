<?xml version="1.0" ?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	
	<xsl:output method="xml" indent="yes"/>
	
	<xsl:key name="attribute-contact-key" match="/plist/array/array/dict/dict/key[. = 'contact']" use="following-sibling::array/string" />
	
	<xsl:template name ="get-key-value">
		<xsl:param name="node"/>
		<xsl:value-of select="$node/following-sibling::*[1]"/>
	</xsl:template>
	
	<xsl:template name="process-name">
		<xsl:param name="node"/>
		<Name>
			<LastName>
				<xsl:call-template name="get-key-value">
					<xsl:with-param name="node" select="$node/key[. = 'last name']" />
				</xsl:call-template>				
			</LastName>
			<FirstName>
				<xsl:call-template name="get-key-value">
					<xsl:with-param name="node" select="$node/key[. = 'first name']" />
				</xsl:call-template>
			</FirstName>
		</Name>
	</xsl:template>
	
	<xsl:template name="process-phone">
		<xsl:param name="node"/>
		<xsl:variable name= "type">
			<xsl:call-template name="get-key-value">
				<xsl:with-param name="node" select="$node/key[. = 'type']" />
			</xsl:call-template>
		</xsl:variable>
		<Telephone>
			<xsl:choose>
				<xsl:when test="$type = 'work'">
					<xsl:attribute name="Location">Work</xsl:attribute>
				</xsl:when>
				<xsl:when test="$type = 'home'">
					<xsl:attribute name="Location">Home</xsl:attribute>
				</xsl:when>
				<xsl:when test="$type = 'mobile'">
					<xsl:attribute name="Type">Cellular</xsl:attribute>
				</xsl:when>
			</xsl:choose>
			<Content>
				<xsl:call-template name="get-key-value">
					<xsl:with-param name="node" select="$node/key[. = 'value']" />
				</xsl:call-template>
			</Content>
		</Telephone>
	</xsl:template>
	
	<xsl:template name="process-email">
		<xsl:param name="node"/>
		<EMail>
			<Content>
				<xsl:call-template name="get-key-value">
					<xsl:with-param name="node" select="$node/key[. = 'value']" />
				</xsl:call-template>
			</Content>
		</EMail>
	</xsl:template>
	
	<xsl:template name="process-address">
		<xsl:param name="node"/>
		<Address>
			<Street>
				<xsl:call-template name="get-key-value">
					<xsl:with-param name="node" select="$node/key[. = 'street']" />
				</xsl:call-template>
			</Street>
			<PostalCode>
				<xsl:call-template name="get-key-value">
					<xsl:with-param name="node" select="$node/key[. = 'postal code']" />
				</xsl:call-template>
			</PostalCode>
		</Address>
	</xsl:template>
	
	<xsl:template name="process-attributes">
		<xsl:param name="node"/>
		<xsl:choose>
			<xsl:when test="substring-before($node/preceding-sibling::*[1], '/') = '3'">
				<xsl:call-template name="process-phone">
					<xsl:with-param name="node" select="$node"/>
				</xsl:call-template>
			</xsl:when>
			<xsl:when test="substring-before($node/preceding-sibling::*[1], '/') = '4'">
				<xsl:call-template name="process-email">
					<xsl:with-param name="node" select="$node"/>
				</xsl:call-template>
			</xsl:when>
			<xsl:when test="substring-before($node/preceding-sibling::*[1], '/') = '5'">
				<xsl:call-template name="process-address">
					<xsl:with-param name="node" select="$node"/>
				</xsl:call-template>
			</xsl:when>
		</xsl:choose>
		
	</xsl:template>
	
	<xsl:template name="process-contact">
		<xsl:param name="dict"/>
		
		<!--Get contact id-->
		<xsl:variable name= "contact-id">
			<xsl:value-of select="$dict/preceding-sibling::*[1]"/>
		</xsl:variable>
		<Uid>
			<content>
				<xsl:value-of select="$contact-id"/>	
			</content>
		</Uid>
		
		<!-- Get Names-->
		<xsl:call-template name="process-name">
			<xsl:with-param name="node" select="$dict"/>
		</xsl:call-template>
		
		<!-- Get other attributes -->
		<xsl:for-each select="key('attribute-contact-key', $contact-id)">
			<xsl:call-template name="process-attributes">
				<xsl:with-param name="node" select="ancestor::*[1]"/>
			</xsl:call-template>
			
		</xsl:for-each>
	</xsl:template>
	
	<!--Process contact references-->
	<xsl:template match="/plist/array/dict/array/dict">
		<contacts>
			<xsl:for-each select="key">
				<contact>
					<xsl:call-template name="process-contact">
						<xsl:with-param name="dict" select="following-sibling::dict[1]"/>
					</xsl:call-template>
				</contact>
			</xsl:for-each>
		</contacts>
	</xsl:template>
	
	<!-- discard every unprocessed node -->
	<xsl:template match="*/text()">
	</xsl:template>
	
</xsl:stylesheet>
