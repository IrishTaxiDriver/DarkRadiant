#pragma once

#include <map>
#include "PropertyEditor.h"
#include <wx/event.h>

class wxBitmapButton;
class wxGridSizer;
class wxCommandEvent;

namespace ui
{

/**
 * \brief
 * Property editor for editing "angle" keys, for setting the direction an entity
 * faces.
 *
 * This property editor provides 8 direction arrow buttons to set the common
 * directions.
 */
class AnglePropertyEditor : 
	public PropertyEditor,
	public wxEvtHandler
{
private:
	typedef std::map<wxBitmapButton*, int> ButtonMap;
	ButtonMap _buttons;

    // Key to edit
    std::string _key;

public:

    /**
     * \brief
     * Default constructor for the factory map.
     */
    AnglePropertyEditor()
    { }

    /**
     * \brief
     * Construct a AnglePropertyEditor to edit the given entity and key.
     */
    AnglePropertyEditor(wxWindow* parent, Entity* entity, const std::string& key);

    /* PropertyEditor implementation */
    IPropertyEditorPtr createNew(wxWindow* parent, Entity* entity,
                                const std::string& key,
                                const std::string& options)
    {
        return PropertyEditorPtr(new AnglePropertyEditor(parent, entity, key));
    }

private:
	// Construct the buttons
    void constructButtons(wxPanel* parent, wxGridSizer* grid);

    // callback
    void _onButtonClick(wxCommandEvent& ev);

	// Helper method to construct an angle button
	wxBitmapButton* constructAngleButton(wxPanel* parent, const std::string& icon, int angleValue);
};

}
