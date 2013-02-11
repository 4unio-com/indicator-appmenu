
from autopilot.testcase import AutopilotTestCase
from autopilot.introspection.gtk import GtkIntrospectionTestMixin
from autopilot.matchers import Eventually
from testtools.matchers import Equals, NotEquals


class GeditTestCase(AutopilotTestCase, GtkIntrospectionTestMixin):

    def setUp(self):
        super(GeditTestCase, self).setUp()
        self.app = self.launch_test_application('gedit')

    def test_about_item(self):
        
        #. Select Help > About
        help_menu_item = self.app.select_single('GtkImageMenuItem', name='HelpMenu')
        self.mouse.move_to_object(help_menu_item)
        self.mouse.click()
        about_menu_item = self.app.select_single('GtkImageMenuItem', name='HelpAboutMenu')
        self.mouse.move_to_object(about_menu_item)
        self.mouse.click()
        self.assertTrue(True)
        #Outcome
        # A small window should appear with the GEdit logo and version in it

        # click on help
        # click on about name="HelpAboutMenu"
        # grok that the modal appears

    def test_syntax_selection(self):
        #. Open Text Editor
        #. Type "int foo;" into the editor
        #. Select View > Highlight Mode > Sources > C
        #Outcome
        #The text 'int' should change color from black

        # type int foo
        # FIXME: is it possible?
        # detect the color of the word int
        pass

    def test_undo_redo_items(self):
        #. Open Text Editor
        #. Type in "foo"
        #. Select Edit > Undo
        #. Select Edit > Redo
        #Outcome
        # The text "foo" should appear when typed.  Disappear when selecting
        # to undo it.  And then repear when selecting redo.
        pass

    def test_close_item(self):
        #. Open Text Editor
        #. Select File > Close
        #
        #Outcome
        # The window should stay open but the default document tab should be
        # closed.
        # FileCloseMenu
        pass


    # OUTMODED BUT HERE FOR REFERENCE
    #def test_toolbar_contains_fileNew(self):
    #    """Gedit toolbar must contain The 'New File' button."""
    #    btn = self.app.select_single('GtkToolButton', name='FileNew')
    #
    #    self.assertThat(btn, NotEquals(None))
    #
    #def test_gedit_starts_with_untitled_document(self):
    #    """Gedit must start with a single edit tab called 'Untitled Document 1'."""
    #    documents = self.app.select_many('GeditTab')
    #
    #    self.assertThat(lambda : len(documents), Eventually(Equals(1)))
    #    self.assertThat(documents[0].name, Equals('Untitled Document 1'))
    #
    #def test_clicking_new_document_starts_new_edit_tab(self):
    #    """Clicking the 'New File' button in the toolbar must start a new document tab."""
    #    btn = self.app.select_single('GtkToolButton', name='FileNew')
    #    self.mouse.move_to_object(btn)
    #    self.mouse.click()
    #
    #    documents = self.app.select_many('GeditTab')
    #    self.assertThat(lambda : len(documents), Eventually(Equals(2)))
    #    self.assertThat(documents[0].name, Equals('Untitled Document 1'))
    #    self.assertThat(documents[1].name, Equals('Untitled Document 2'))
