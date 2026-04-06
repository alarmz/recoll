<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"
 >

 <xsl:output omit-xml-declaration="yes"/>

 <xsl:template match="/">
  <div>
  <xsl:apply-templates/> 
  </div>
</xsl:template>

 <xsl:template match="Text">
  <p>
  <xsl:value-of select="."/>
  </p>
 </xsl:template>



</xsl:stylesheet>

