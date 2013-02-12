noinst_LTLIBRARIES = libdesktopwebapp_npapi_plugin.la

libdesktopwebapp_npapi_plugin_la_CPPFLAGS = \
	$(DESKTOPWEBAPP_NPAPI_PLUGIN_CFLAGS) \
	$(NPAPI_DEBUG_CFLAGS) \
	-DG_LOG_DOMAIN=\"desktop-webapp-npapi\" \
	-DXP_UNIX=1

libdesktopwebapp_npapi_plugin_la_SOURCES = \
	npapi-headers/headers/npapi.h \
	npapi-headers/headers/npfunctions.h \
	npapi-headers/headers/npruntime.h \
	npapi-headers/headers/nptypes.h \
	object.c \
	object.h \
	plugin.c \
	plugin.h

libdesktopwebapp_npapi_plugin_la_LDFLAGS = \
        -avoid-version \
	-module \
	-shared \
	-export-dynamic \
	-rpath /nowhere

libdesktopwebapp_npapi_plugin_la_LIBADD = \
	$(DESKTOPWEBAPP_NPAPI_PLUGIN_LIBS)

noinst_HEADERS = \
	npapi-headers/headers/npapi.h		\
	npapi-headers/headers/npfunctions.h	\
	npapi-headers/headers/npruntime.h	\
	npapi-headers/headers/nptypes.h
