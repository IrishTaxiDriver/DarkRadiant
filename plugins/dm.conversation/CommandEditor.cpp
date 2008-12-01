#include "CommandEditor.h"

#include <gtk/gtk.h>
#include "gtkutil/LeftalignedLabel.h"
#include "gtkutil/LeftAlignment.h"
#include "gtkutil/RightAlignment.h"
#include "gtkutil/TreeModel.h"
#include "string/string.h"

#include "itextstream.h"

#include "ConversationCommandLibrary.h"

namespace ui {

	namespace {
		const std::string WINDOW_TITLE = "Edit Command";
	}

CommandEditor::CommandEditor(GtkWindow* parent, conversation::ConversationCommand& command, conversation::Conversation conv) :
	gtkutil::BlockingTransientWindow(WINDOW_TITLE, parent),
	_conversation(conv),
	_command(command),
	_result(NUM_RESULTS),
	_actorStore(gtk_list_store_new(2, G_TYPE_INT, G_TYPE_STRING)), // number + caption
	_commandStore(gtk_list_store_new(2, G_TYPE_INT, G_TYPE_STRING)), // number + caption
	_argTable(NULL)
{
	gtk_container_set_border_width(GTK_CONTAINER(getWindow()), 12);

	// Fill the actor store
	for (conversation::Conversation::ActorMap::const_iterator i = _conversation.actors.begin();
		 i != _conversation.actors.end(); ++i)
	{
		GtkTreeIter iter;
		gtk_list_store_append(_actorStore, &iter);
		gtk_list_store_set(_actorStore, &iter, 
						   0, i->first, 
						   1, (std::string("Actor ") + intToStr(i->first) + " (" + i->second + ")").c_str(),
						   -1);
	}

	// Let the command library fill the command store
	conversation::ConversationCommandLibrary::Instance().populateListStore(_commandStore);

	// Create all widgets
	populateWindow();

	// Fill the values
	updateWidgets();

	// Show the editor and block
	show();
}

CommandEditor::Result CommandEditor::getResult() {
	return _result;
}

void CommandEditor::updateWidgets() {
	// Select the actor passed from the command
	gtkutil::TreeModel::SelectionFinder finder(_command.actor, 0);

	gtk_tree_model_foreach(
		GTK_TREE_MODEL(_actorStore), 
		gtkutil::TreeModel::SelectionFinder::forEach, 
		&finder
	);
	
	// Select the found treeiter, if the name was found in the liststore
	if (finder.getPath() != NULL) {
		GtkTreeIter iter = finder.getIter();
		gtk_combo_box_set_active_iter(GTK_COMBO_BOX(_actorDropDown), &iter);
	}

	// Select the type passed from the command
	gtkutil::TreeModel::SelectionFinder cmdFinder(_command.type, 0);

	gtk_tree_model_foreach(
		GTK_TREE_MODEL(_commandStore), 
		gtkutil::TreeModel::SelectionFinder::forEach, 
		&cmdFinder
	);
	
	// Select the found treeiter, if the name was found in the liststore
	if (cmdFinder.getPath() != NULL) {
		GtkTreeIter iter = cmdFinder.getIter();
		gtk_combo_box_set_active_iter(GTK_COMBO_BOX(_commandDropDown), &iter);
	}
}

void CommandEditor::save() {
	// TODO
}

void CommandEditor::populateWindow() {
	// Create the overall vbox
	GtkWidget* vbox = gtk_vbox_new(FALSE, 6);

	// Actor
	gtk_box_pack_start(GTK_BOX(vbox), gtkutil::LeftAlignedLabel("<b>Actor</b>"), FALSE, FALSE, 0);

	// Create the actor dropdown box
	_actorDropDown = gtk_combo_box_new_with_model(GTK_TREE_MODEL(_actorStore));

	// Add the cellrenderer for the name
	GtkCellRenderer* nameRenderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(_actorDropDown), nameRenderer, TRUE);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(_actorDropDown), nameRenderer, "text", 1);
	
	gtk_box_pack_start(GTK_BOX(vbox), gtkutil::LeftAlignment(_actorDropDown, 18, 1), FALSE, FALSE, 0);

	// Command Type
	gtk_box_pack_start(GTK_BOX(vbox), gtkutil::LeftAlignedLabel("<b>Command</b>"), FALSE, FALSE, 0);
	_commandDropDown = gtk_combo_box_new_with_model(GTK_TREE_MODEL(_commandStore));

	// Connect the signal to get notified of further changes
	g_signal_connect(G_OBJECT(_commandDropDown), "changed", G_CALLBACK(onCommandTypeChange) , this);

	// Add the cellrenderer for the name
	GtkCellRenderer* cmdNameRenderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(_commandDropDown), cmdNameRenderer, TRUE);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(_commandDropDown), cmdNameRenderer, "text", 1);

	gtk_box_pack_start(GTK_BOX(vbox), gtkutil::LeftAlignment(_commandDropDown, 18, 1), FALSE, FALSE, 0);
	
	// Command Arguments
	gtk_box_pack_start(GTK_BOX(vbox), gtkutil::LeftAlignedLabel("<b>Command Arguments</b>"), FALSE, FALSE, 0);
	
	// Create the alignment container that hold the (exchangable) widget table
	_argAlignment = gtk_alignment_new(0.0, 0.5, 1.0, 1.0);
	gtk_alignment_set_padding(GTK_ALIGNMENT(_argAlignment), 0, 0, 18, 0);

	gtk_box_pack_start(GTK_BOX(vbox), _argAlignment, FALSE, FALSE, 3);

	// Buttons
	gtk_box_pack_start(GTK_BOX(vbox), createButtonPanel(), FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(getWindow()), vbox);
}

void CommandEditor::commandTypeChanged() {
	int newCommandTypeID = -1;
	
	// Get the currently selected effect name from the combo box
	GtkTreeIter iter;
	GtkComboBox* combo = GTK_COMBO_BOX(_commandDropDown);
	
	if (gtk_combo_box_get_active_iter(combo, &iter)) {
		GtkTreeModel* model = gtk_combo_box_get_model(combo);
		
		newCommandTypeID = gtkutil::TreeModel::getInt(model, &iter, 0);
	}

	// Create the argument widgets for this new command type
	createArgumentWidgets(newCommandTypeID);
}

void CommandEditor::createArgumentWidgets(int commandTypeID) {

	const conversation::ConversationCommandInfo& cmdInfo = 
		conversation::ConversationCommandLibrary::Instance().findCommandInfo(commandTypeID);

	// Remove all possible previous items from the list
	_argumentItems.clear();

	// Remove the old table if there exists one
	if (_argTable != NULL) {
		// This removes the old table from the alignment container
		// greebo: Increase the refCount of the table to prevent destruction.
		// Destruction would cause weird shutdown crashes.
		g_object_ref(G_OBJECT(_argTable));
		gtk_container_remove(GTK_CONTAINER(_argAlignment), _argTable);
	}
	
	// Create the tooltips group for the help mouseover texts
	_tooltips = gtk_tooltips_new();
	gtk_tooltips_enable(_tooltips);
	
	// Setup the table with default spacings
	_argTable = gtk_table_new(static_cast<guint>(cmdInfo.arguments.size()), 3, false);
    gtk_table_set_col_spacings(GTK_TABLE(_argTable), 12);
    gtk_table_set_row_spacings(GTK_TABLE(_argTable), 6);
	gtk_container_add(GTK_CONTAINER(_argAlignment), _argTable); 

	typedef conversation::ConversationCommandInfo::ArgumentInfoList::const_iterator ArgumentIter;

	int index = 0;

	for (ArgumentIter i = cmdInfo.arguments.begin(); 
		 i != cmdInfo.arguments.end(); ++i, ++index)
	{
		const conversation::ArgumentInfo& argInfo = *i;

		CommandArgumentItemPtr item;
		
		switch (argInfo.type)
		{
		case conversation::ArgumentInfo::ARGTYPE_INT:
		case conversation::ArgumentInfo::ARGTYPE_FLOAT:
		case conversation::ArgumentInfo::ARGTYPE_STRING:
			// Create a new string argument item
			item = CommandArgumentItemPtr(new StringArgument(argInfo, _tooltips));
			break;
		case conversation::ArgumentInfo::ARGTYPE_VECTOR:
		case conversation::ArgumentInfo::ARGTYPE_SOUNDSHADER:
			// Create a new string argument item
			item = CommandArgumentItemPtr(new StringArgument(argInfo, _tooltips));
			break;
		case conversation::ArgumentInfo::ARGTYPE_ACTOR:
			// Create a new actor argument item
			item = CommandArgumentItemPtr(new ActorArgument(argInfo, _tooltips, _actorStore));
			break;
		case conversation::ArgumentInfo::ARGTYPE_ENTITY:
			// Create a new string argument item
			item = CommandArgumentItemPtr(new StringArgument(argInfo, _tooltips));
			break;
		default:
			globalErrorStream() << "Unknown command argument type: " << argInfo.type << std::endl;
			break;
		};

		if (item != NULL) {
			_argumentItems.push_back(item);
			
			/*if (arg.type != "b") {
				// The label
				gtk_table_attach(
					GTK_TABLE(_argTable), item->getLabelWidget(),
					0, 1, index-1, index, // index starts with 1, hence the -1
					GTK_FILL, (GtkAttachOptions)0, 0, 0
				);
				
				// The edit widgets
				gtk_table_attach_defaults(
					GTK_TABLE(_argTable), item->getEditWidget(),
					1, 2, index-1, index // index starts with 1, hence the -1
				);
			}
			else {
				// This is a checkbutton - should be spanned over two columns
				gtk_table_attach(
					GTK_TABLE(_argTable), item->getEditWidget(),
					0, 2, index-1, index, // index starts with 1, hence the -1
					GTK_FILL, (GtkAttachOptions)0, 0, 0
				);
			}
			
			// The help widgets
			gtk_table_attach(
				GTK_TABLE(_argTable), item->getHelpWidget(),
				2, 3, index-1, index, // index starts with 1, hence the -1
				(GtkAttachOptions)0, (GtkAttachOptions)0, 0, 0
			);*/
		}
	}
	
	// Show the table and all subwidgets
	gtk_widget_show_all(_argTable);
}

GtkWidget* CommandEditor::createButtonPanel() {
	GtkWidget* buttonHBox = gtk_hbox_new(TRUE, 12);
	
	// Save button
	GtkWidget* okButton = gtk_button_new_from_stock(GTK_STOCK_OK);
	g_signal_connect(G_OBJECT(okButton), "clicked", G_CALLBACK(onSave), this);
	gtk_box_pack_end(GTK_BOX(buttonHBox), okButton, TRUE, TRUE, 0);
	
	// Cancel Button
	GtkWidget* cancelButton = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	g_signal_connect(G_OBJECT(cancelButton), "clicked", G_CALLBACK(onCancel), this);
	gtk_box_pack_end(GTK_BOX(buttonHBox), cancelButton, TRUE, TRUE, 0);
	
	return gtkutil::RightAlignment(buttonHBox);	
}

void CommandEditor::onSave(GtkWidget* button, CommandEditor* self) {
	// First, save to the command object
	self->_result = RESULT_OK;
	self->save();
	
	// Then close the window
	self->destroy();
}

void CommandEditor::onCancel(GtkWidget* button, CommandEditor* self) {
	// Just close the window without writing the values
	self->_result = RESULT_CANCEL;
	self->destroy();
}

void CommandEditor::onCommandTypeChange(GtkWidget* combobox, CommandEditor* self) {
	self->commandTypeChanged();
}

} // namespace ui
