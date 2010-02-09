<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="text"/>
  <xsl:template match="/">
    #include &quot;<xsl:value-of select="//@name"/>.h&quot;
    bool ipc_process_<xsl:value-of select="//@name"/>(struct ipc *ipc)
    {
    uint32_t id;
    if (!ipc_read_uint32_t(ipc, &amp;id))
    return false;
    switch(id){
    <xsl:apply-templates/>
    default:
    return false;
    }
    mpool_cleanup(&amp;ipc-&gt;mp);
    return ipc-&gt;ok &amp;&amp; ipc_flush(ipc);
    }
</xsl:template>
  <xsl:template match="func">
    case <xsl:value-of select="@id"/>: {
    <xsl:for-each select="*">
      <xsl:value-of select="@type"/><xsl:text> </xsl:text>
      <xsl:value-of select="@name"/>;
    </xsl:for-each>
    int32_t result;
    ipc-&gt;ok = (
    <xsl:for-each select="in">
      ipc_read_<xsl:value-of select="@type"/>
      (ipc, &amp;<xsl:value-of select="@name"/>) &amp;&amp;
    </xsl:for-each>
    ((result = <xsl:value-of select="@name"/>(ipc
    <xsl:apply-templates/>)), ipc_write_int32_t(ipc, &amp;result)));
    if (ipc-&gt;ok &amp;&amp; result == 0) {
    ipc-&gt;ok = ipc-&gt;ok
    <xsl:for-each select="out">
      &amp;&amp; ipc_write_<xsl:value-of select="@type"/>
      (ipc, &amp;<xsl:value-of select="@name"/>)
    </xsl:for-each>;
    }
    }
    break;
  </xsl:template>
  <xsl:template match="in">
    , &amp;<xsl:value-of select="@name"/>
  </xsl:template>
  <xsl:template match="out">
    , &amp;<xsl:value-of select="@name"/>
  </xsl:template>
</xsl:stylesheet>
