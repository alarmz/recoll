<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" 
xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" 
xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"
 >

 <xsl:output omit-xml-declaration="yes"/>

 <xsl:template match="/">
  <div>
  <xsl:apply-templates/> 
  </div>
</xsl:template>

 <xsl:template match="a:t">
  <p>
  <xsl:value-of select="."/>
  </p>
 </xsl:template>

<xsl:template match="p:attrName">
</xsl:template>


</xsl:stylesheet>

