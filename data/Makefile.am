iconsdir = \
	$(datadir)/icons/hicolor/scalable/status

icons_DATA = \
	indicator-appmenu-menu-panel.svg

gsettings_SCHEMAS = \
    com.canonical.indicator.appmenu.gschema.xml

@GSETTINGS_RULES@

EXTRA_DIST = \
	$(icons_DATA) \
	$(gsettings_SCHEMAS)

CLEANFILES = $(gsettings_SCHEMAS:.xml=.valid)
