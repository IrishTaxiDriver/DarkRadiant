#pragma once

#include "ui/ientityinspector.h"
#include <wx/event.h>

namespace ui
{

	namespace
	{
		const std::string DEF_HEAD_KEY = "def_head";
	}

class AIHeadPropertyEditor final :
	public wxEvtHandler,
	public IPropertyEditor
{
private:
	// The top-level widget
	wxPanel* _widget;

    IEntitySelection& _entities;

    ITargetKey::Ptr _key;

public:
	~AIHeadPropertyEditor();

	wxPanel* getWidget() override;
	void updateFromEntities();

	AIHeadPropertyEditor(wxWindow* parent, IEntitySelection& entities, const ITargetKey::Ptr& key);

    static Ptr CreateNew(wxWindow* parent, IEntitySelection& entities, const ITargetKey::Ptr& key);

private:
	void onChooseButton(wxCommandEvent& ev);
};

class AIHeadEditorDialogWrapper :
    public IPropertyEditorDialog
{
public:
    std::string runDialog(Entity* entity, const std::string& key) override;
};

} // namespace ui
