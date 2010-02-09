%.h: %.xml
	xsltproc $(XSLTFLAGS) -o $@ header.xsl $<

%.client.c: %.xml %.h
	xsltproc $(XSLTFLAGS) -o $@ client.xsl $<

%.server.c: %.xml %.h
	xsltproc $(XSLTFLAGS) -o $@ server.xsl $<
