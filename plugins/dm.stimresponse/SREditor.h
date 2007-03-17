#ifndef SREDITOR_H_
#define SREDITOR_H_

#include "iselection.h"
#include "ientity.h"
#include "gtkutil/WindowPosition.h"

#include "StimTypes.h"
#include "SREntity.h"

typedef struct _GtkTreeView GtkTreeView;
typedef struct _GtkToggleButton GtkToggleButton;
typedef struct _GtkTreeSelection GtkTreeSelection;
typedef struct _GtkComboBox GtkComboBox;
typedef struct _GtkEditable GtkEditable;
typedef struct _GtkCellRendererText GtkCellRendererText;

namespace ui {

class StimResponseEditor :
	public SelectionSystem::Observer
{
	GtkWidget* _dialog;
	
	GtkWidget* _dialogVBox;
	
	// The combobox using a liststore model filled with stims
	struct AddWidgets {
		GtkWidget* stimTypeList;
		GtkWidget* addButton;
		GtkWidget* addScriptButton;
	} _addWidgets; 
	
	// The treeview with the entity's stims/responses
	GtkWidget* _entitySRView;
	GtkTreeSelection* _entitySRSelection;
	
	struct SRPropertyWidgets {
		GtkWidget* vbox;
		GtkWidget* typeList;
		GtkWidget* stimButton;
		GtkWidget* respButton;
		GtkWidget* active;
		GtkWidget* useBounds;
		GtkWidget* radiusToggle;
		GtkWidget* radiusEntry;
		GtkWidget* timeIntToggle;
		GtkWidget* timeIntEntry;
		GtkWidget* modelToggle;
		GtkWidget* modelEntry;
	} _srWidgets;
	
	struct ScriptWidgets {
		GtkWidget* view;
		GtkTreeSelection* selection;
	} _scriptWidgets;
	
	// The list of the entity's stims/responses
	SREntityPtr _srEntity;
	
	// The position/size memoriser
	gtkutil::WindowPosition _windowPosition;
	
	// The entity we're editing
	Entity* _entity;
	
	// The helper class managing the stims
	StimTypes _stimTypes;
	
	// To allow updating the widgets without firing callbacks
	bool _updatesDisabled;

public:
	StimResponseEditor();
	
	/** greebo: Contains the static instance of this dialog.
	 */
	static StimResponseEditor& Instance();
	
	// Command target to toggle the dialog
	static void toggle();
	
	/** greebo: Some sort of "soft" destructor that de-registers
	 * this class from the SelectionSystem, saves the window state, etc.
	 */
	void shutdown();
	
	/** greebo: Gets called by the SelectionSystem
	 */
	void selectionChanged(scene::Instance& instance);

private:

	/** greebo: Discards the changes and re-loads everything from the entity.
	 */
	void revert();

	/** greebo: Saves the current working set to the entity
	 */
	void save();

	/** greebo: Retrieves the currently selected stimType name
	 * 
	 * @returns: the name of the Stim type (e.g. STIM_FIRE)
	 */
	std::string getStimTypeName();

	/** greebo: Adds an empty response script to the list.
	 */
	void addResponseScript();

	/** greebo: Removes the currently selected script
	 */
	void removeScript();

	/** greebo: Removes the currently selected stim/response object
	 */
	void removeStimResponse();

	/** greebo: Adds a new StimResponse object, the index and the internal
	 * 			id are auto-incremented. The ListStore is refreshed. 
	 */
	void addStimResponse();

	/** greebo: Returns the ID of the currently selected stim
	 * 		
	 * @returns: the id (number) of the selected stim or -1 on failure 
	 */
	int getIdFromSelection();

	/** greebo: Tries to set the <key> of the currently selected 
	 * 			StimResponse to <value>
	 * 			The request is refused for inherited StimResponses.
	 */
	void setProperty(const std::string& key, const std::string& value);

	/** greebo: Updates the SR widget group according to the list selection.
	 */
	void updateSRWidgets();

	/** greebo: Creates the S/R widget group and returns its vbox
	 */
	GtkWidget* createSRWidgets();

	/** greebo: This fills the window with widgets
	 */
	void populateWindow();
	
	/** greebo: This updates the widget sensitivity and loads
	 * 			the data into them.
	 */
	void update();

	/** greebo: Updates the sensitivity of the "Add Response Script" button
	 */
	void updateAddScriptButton();

	/** greebo: Checks the selection for a single entity.
	 */
	void rescanSelection();

	/** greebo: This toggles the visibility of the surface dialog.
	 * The dialog is constructed only once and never destructed 
	 * during runtime.
	 */
	void toggleWindow();
	
	// The callback for the delete event (toggles the visibility)
	static gboolean onDelete(GtkWidget* widget, GdkEvent* event, StimResponseEditor* self);

	// Callback for Stim/Response selection changes
	static void onSelectionChange(GtkTreeSelection* treeView, StimResponseEditor* self);
	static void onClassChange(GtkToggleButton* toggleButton, StimResponseEditor* self);
	static void onTypeSelect(GtkComboBox* widget, StimResponseEditor* self);
	
	// StimType selection change
	static void onStimTypeChange(GtkComboBox* widget, StimResponseEditor* self);
	
	// "Active" property
	static void onActiveToggle(GtkToggleButton* toggleButton, StimResponseEditor* self);
	static void onBoundsToggle(GtkToggleButton* toggleButton, StimResponseEditor* self);
	static void onRadiusToggle(GtkToggleButton* toggleButton, StimResponseEditor* self);
	static void onModelToggle(GtkToggleButton* toggleButton, StimResponseEditor* self);
	static void onTimeIntervalToggle(GtkToggleButton* toggleButton, StimResponseEditor* self);

	// Entry text changes
	static void onModelChanged(GtkEditable* editable, StimResponseEditor* self);
	static void onTimeIntervalChanged(GtkEditable* editable, StimResponseEditor* self);
	static void onRadiusChanged(GtkEditable* editable, StimResponseEditor* self);

	// "Add" Stim/Response
	static void onAdd(GtkWidget* button, StimResponseEditor* self);
	static void onSave(GtkWidget* button, StimResponseEditor* self);
	static void onRevert(GtkWidget* button, StimResponseEditor* self);
	static void onScriptAdd(GtkWidget* button, StimResponseEditor* self);

	// Gets notified if a cell has been changed.
	static void onScriptEdit(GtkCellRendererText* renderer, 
							 gchar* path, gchar* new_text, StimResponseEditor* self);
	
	// The keypress handler for catching the keys when in the treeview
	static gboolean onTreeViewKeyPress(GtkTreeView* view, GdkEventKey* event, StimResponseEditor* self);

}; // class StimResponseEditor

} // namespace ui

#endif /*SREDITOR_H_*/
