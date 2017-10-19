UBERJAR = target/defold-editor-2.0.0-SNAPSHOT-standalone.jar

$(UBERJAR):
	lein uberjar

defold.classlist_original: $(UBERJAR)
	-java -XX:+UnlockCommercialFeatures \
		-XX:+UseAppCDS \
		-Xshare:off \
		-XX:DumpLoadedClassList=$@ \
		-jar $(UBERJAR) \
		foobar

defold.classlist: defold.classlist_original
	cat $< \
		| grep -v 'org/apache/commons/logging' \
		| grep -v 'org/eclipse/jface/text' \
		| grep -v 'com/defold/editor/eclipse' \
		| grep -v 'org/apache/commons/lang' \
		| grep -v 'org/apache/commons/configuration' \
		| grep -v 'org/eclipse/fx/text' \
		| grep -v 'org/eclipse/fx/ui' \
		| grep -v 'com/defold/libs/ResourceUnpacker' \
		> $@

defold.cache: $(UBERJAR) defold.classlist
	java -XX:+UnlockCommercialFeatures \
		-XX:+UseAppCDS \
		-Xshare:dump \
		-XX:SharedClassListFile=defold.classlist \
		-XX:SharedArchiveFile=$@ \
		-jar $(UBERJAR) \
		foobar

run-cds: $(UBERJAR) defold.cache
	time -lp java -XX:+UnlockCommercialFeatures \
		-XX:+UseAppCDS \
		-Xshare:on \
		-XX:SharedArchiveFile=defold.cache \
		-cp $(UBERJAR) \
		com.defold.editor.Start \
		foobar
.PHONY: run-cds

run-off: $(UBERJAR) defold.cache
	time -lp java -XX:+UnlockCommercialFeatures \
		-XX:+UseAppCDS \
		-Xshare:off \
		-XX:SharedArchiveFile=defold.cache \
		-cp $(UBERJAR) \
		com.defold.editor.Start \
		foobar
.PHONY: run-off
