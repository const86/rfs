<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="text"/>
  <xsl:template match="/">
    #include &quot;<xsl:value-of select="//@name"/>.h&quot;
    <xsl:apply-templates/>
  </xsl:template>
  <xsl:template match="func">
    int32_t <xsl:value-of select="@name"/>
    (struct ipc *ipc <xsl:apply-templates/>)
    {
    const uint32_t id = UINT32_C(<xsl:value-of select="@id"/>);
    int32_t <xsl:value-of select="@name"/>;
    ipc-&gt;ok = (ipc_write_uint32_t(ipc, &amp;id)
    <xsl:for-each select="in">
      &amp;&amp; ipc_write_<xsl:value-of select="@type"/>
      (ipc, <xsl:value-of select="@name"/>)
    </xsl:for-each>
    &amp;&amp; ipc_flush(ipc)
    &amp;&amp; ipc_read_int32_t(ipc, &amp;<xsl:value-of select="@name"/>)
    &amp;&amp; ((<xsl:value-of select="@name"/> != 0) || (
    <xsl:for-each select="out">
      ipc_read_<xsl:value-of select="@type"/>
      (ipc, <xsl:value-of select="@name"/>) &amp;&amp;
    </xsl:for-each> true)));
    return ipc-&gt;ok ? <xsl:value-of select="@name"/> : INT32_C(-1);
    }
  </xsl:template>
  <xsl:template match="in">
    , const <xsl:value-of select="@type"/>
    *<xsl:text> </xsl:text><xsl:value-of select="@name"/>
  </xsl:template>
  <xsl:template match="out">
    , <xsl:value-of select="@type"/> *<xsl:value-of select="@name"/>
  </xsl:template>
</xsl:stylesheet>
