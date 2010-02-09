<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="text"/>
  <xsl:template match="/">
    #include &lt;ipc.h&gt;
    extern bool ipc_process_<xsl:value-of select="//@name"/>(struct ipc *ipc);
    <xsl:apply-templates/>
  </xsl:template>
  <xsl:template match="alias">
    typedef <xsl:value-of select="@type"/>
    <xsl:text> </xsl:text><xsl:value-of select="@name"/>;
    static inline bool ipc_read_<xsl:value-of select="@name"/>
    (struct ipc *ipc, <xsl:value-of select="@name"/> *p){
    return ipc_read_<xsl:value-of select="@type"/>(ipc, p);
    }
    static inline bool ipc_write_<xsl:value-of select="@name"/>
    (struct ipc *ipc, const <xsl:value-of select="@name"/> *p){
    return ipc_write_<xsl:value-of select="@type"/>(ipc, p);
    }
  </xsl:template>
  <xsl:template match="type">
    typedef struct <xsl:value-of select="@name"/> {
    <xsl:for-each select="field">
      <xsl:value-of select="@type"/><xsl:text> </xsl:text>
      <xsl:value-of select="@name"/>;
    </xsl:for-each>
    } <xsl:value-of select="@name"/>;
    static inline bool ipc_read_<xsl:value-of select="@name"/>
    (struct ipc *ipc, <xsl:value-of select="@name"/> *p){
    return (
    <xsl:for-each select="field">
      ipc_read_<xsl:value-of select="@type"/>
      (ipc, &amp;p-&gt;<xsl:value-of select="@name"/>) &amp;&amp;
    </xsl:for-each> true);
    }
    static inline bool ipc_write_<xsl:value-of select="@name"/>
    (struct ipc *ipc, const <xsl:value-of select="@name"/> *p){
    return (
    <xsl:for-each select="field">
      ipc_write_<xsl:value-of select="@type"/>
      (ipc, &amp;p-&gt;<xsl:value-of select="@name"/>) &amp;&amp;
    </xsl:for-each> true);
    }
  </xsl:template>
  <xsl:template match="list">
    typedef struct list_<xsl:value-of select="@type"/> {
    uint32_t n;
    uint32_t _n;
    <xsl:value-of select="@type"/> *p;
    } list_<xsl:value-of select="@type"/>;
    static inline bool ipc_read_list_<xsl:value-of select="@type"/>
    (struct ipc *ipc, list_<xsl:value-of select="@type"/> *p)
    {
    if (!ipc_read_uint32_t(ipc, &amp;p-&gt;n))
    return false;
    if (p-&gt;n == 0)
    return true;
    p-&gt;p = mpool_alloc(&amp;ipc-&gt;mp,
    sizeof(<xsl:value-of select="@type"/>) * p-&gt;n);
    if (p-&gt;p == NULL)
    return false;
    for (<xsl:value-of select="@type"/> *q = p-&gt;p;
    q &lt; p-&gt;p + p-&gt;n; q++) {
    if (!ipc_read_<xsl:value-of select="@type"/>(ipc, q))
    return false;
    }
    p-&gt;_n = p-&gt;n;
    return true;
    }
    static inline bool ipc_write_list_<xsl:value-of select="@type"/>
    (struct ipc *ipc, const list_<xsl:value-of select="@type"/> *p)
    {
    if (!ipc_write_uint32_t(ipc, &amp;p-&gt;n))
    return false;
    for (const <xsl:value-of select="@type"/> *q = p-&gt;p;
    q &lt; p-&gt;n + p-&gt;p; q++) {
    if (!ipc_write_<xsl:value-of select="@type"/>(ipc, q))
    return false;
    }
    return true;
    }
    static inline bool list_append_<xsl:value-of select="@type"/>
    (struct mpool *mp, list_<xsl:value-of select="@type"/> *p,
    const <xsl:value-of select="@type"/> *x)
    {
    if (p-&gt;n &gt;= p-&gt;_n) {
    uint32_t _n = (p-&gt;_n == 0) ? 128 : (p-&gt;_n &lt;&lt; 1);
    <xsl:value-of select="@type"/> *_p =
    mpool_realloc(mp, p-&gt;p, sizeof(<xsl:value-of select="@type"/>) * _n);
    if (_p == NULL)
    return false;
    p-&gt;p = _p;
    p-&gt;_n = _n;
    }
    p-&gt;p[p-&gt;n++] = *x;
    return true;
    }
  </xsl:template>
  <xsl:template match="func">
    int32_t <xsl:value-of select="@name"/>
    (struct ipc *ipc <xsl:apply-templates/>);
  </xsl:template>
  <xsl:template match="in">
    , const <xsl:value-of select="@type"/><xsl:text> </xsl:text>
    *<xsl:value-of select="@name"/>
  </xsl:template>
  <xsl:template match="out">
    , <xsl:value-of select="@type"/> *<xsl:value-of select="@name"/>
  </xsl:template>
</xsl:stylesheet>
