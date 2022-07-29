#include "MD5AnimationViewer.h"

#include "i18n.h"
#include "imodelcache.h"
#include "imd5anim.h"

#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/sizer.h>

#include "ifavourites.h"
#include "wxutil/Bitmap.h"
#include "wxutil/dataview/ResourceTreeViewToolbar.h"
#include "wxutil/dataview/ThreadedResourceTreePopulator.h"
#include "wxutil/dataview/TreeViewItemStyle.h"

namespace ui
{

/**
 * Visitor class to retrieve modelDefs and add them to folders.
 */
class ThreadedModelDefLoader final :
    public wxutil::ThreadedResourceTreePopulator
{
private:
    const wxutil::DeclarationTreeView::Columns& _columns;

    std::set<std::string> _favourites;

    wxIcon _folderIcon;
    wxIcon _modelIcon;

public:
    ThreadedModelDefLoader(const wxutil::DeclarationTreeView::Columns& columns) :
        ThreadedResourceTreePopulator(columns),
        _columns(columns)
    {
        // Get the list of favourites
        _favourites = GlobalFavouritesManager().getFavourites(decl::getTypeName(decl::Type::Skin));

        _modelIcon.CopyFromBitmap(wxutil::GetLocalBitmap("model16green.png"));
        _folderIcon.CopyFromBitmap(wxutil::GetLocalBitmap("folder16.png"));
    }

    ~ThreadedModelDefLoader()
    {
        EnsureStopped();
    }

protected:
    void PopulateModel(const wxutil::TreeModel::Ptr& model) override
    {
        wxutil::VFSTreePopulator populator(model);

        GlobalEntityClassManager().forEachModelDef([&](const IModelDef::Ptr& modelDef)
        {
            populator.addPath(modelDef->getModName() + "/" + modelDef->getDeclName(), [&](wxutil::TreeModel::Row& row,
                const std::string& path, const std::string& leafName, bool isFolder)
            {
                StoreModelDefValues(row, modelDef->getDeclName(), leafName, isFolder);
            });;
        });
    }

    void SortModel(const wxutil::TreeModel::Ptr& model) override
    {
        model->SortModelFoldersFirst(_columns.leafName, _columns.isFolder);
    }

private:
    void StoreModelDefValues(wxutil::TreeModel::Row& row, const std::string& modelName, const std::string& leafName, bool isFolder)
    {
        bool isFavourite = _favourites.count(modelName) > 0;

        row[_columns.iconAndName] = wxVariant(wxDataViewIconText(leafName, !isFolder ? _modelIcon : _folderIcon));
        row[_columns.iconAndName] = wxutil::TreeViewItemStyle::Declaration(isFavourite);
        row[_columns.fullName] = modelName;
        row[_columns.leafName] = leafName;
        row[_columns.declName] = modelName;
        row[_columns.isFolder] = isFolder;
        row[_columns.isFavourite] = isFavourite;

        row.SendItemAdded();
    }
};

MD5AnimationViewer::MD5AnimationViewer(wxWindow* parent, RunMode runMode) :
	wxutil::DialogBase(_("MD5 Animation Viewer"), parent),
	_runMode(runMode),
	_animList(new wxutil::TreeModel(_animColumns, true))
{
	SetSizer(new wxBoxSizer(wxVERTICAL));

	auto splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
	splitter->SetMinimumPaneSize(10); // disallow unsplitting

	// Preview goes to the right
	_preview.reset(new AnimationPreview(splitter));

	splitter->SplitVertically(createListPane(splitter), _preview->getWidget());

	GetSizer()->Add(splitter, 1, wxEXPAND | wxALL, 12);

	if (_runMode == RunMode::Selection)
	{
		GetSizer()->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxALIGN_RIGHT | wxBOTTOM | wxRIGHT, 12);
		SetAffirmativeId(wxID_OK);
	}
	else
	{
		GetSizer()->Add(CreateStdDialogButtonSizer(wxCLOSE), 0, wxALIGN_RIGHT | wxBOTTOM | wxRIGHT, 12);
		SetAffirmativeId(wxID_CLOSE);
	}

	FitToScreen(0.8f, 0.7f);

	// Set the default size of the window
	splitter->SetSashPosition(static_cast<int>(GetSize().GetWidth() * 0.25f));

	// Populate with model names
	populateModelList();

	Bind(wxEVT_IDLE, [&](wxIdleEvent& ev)
	{
		ev.Skip();

		if (!_animToSelect.empty())
		{
			setSelectedAnim(_animToSelect);
		}
	});
}

void MD5AnimationViewer::Show(const cmd::ArgumentList& args)
{
	auto viewer = new MD5AnimationViewer(nullptr, RunMode::ViewOnly);

	viewer->ShowModal();
	viewer->Destroy();
}

wxWindow* MD5AnimationViewer::createListPane(wxWindow* parent)
{
	auto listPane = new wxPanel(parent, wxID_ANY);
	listPane->SetSizer(new wxBoxSizer(wxVERTICAL));

    auto modelLabel = new wxStaticText(listPane, wxID_ANY, _("Model Definition"));
	modelLabel->SetFont(modelLabel->GetFont().Bold());

    auto animLabel = new wxStaticText(listPane, wxID_ANY, _("Available Animations"));
	animLabel->SetFont(animLabel->GetFont().Bold());
	
	listPane->GetSizer()->Add(modelLabel, 0, wxEXPAND | wxBOTTOM, 6);
	listPane->GetSizer()->Add(createModelTreeView(listPane), 1, wxEXPAND | wxBOTTOM | wxTOP, 6);
	listPane->GetSizer()->Add(animLabel, 0, wxEXPAND | wxBOTTOM | wxTOP, 6);
	listPane->GetSizer()->Add(createAnimTreeView(listPane), 1, wxEXPAND | wxBOTTOM | wxTOP, 6);

	return listPane;
}

wxWindow* MD5AnimationViewer::createModelTreeView(wxWindow* parent)
{
    auto panel = new wxPanel(parent);
    panel->SetSizer(new wxBoxSizer(wxVERTICAL));

    _modelTreeView = new wxutil::DeclarationTreeView(panel, decl::Type::ModelDef, _modelColumns, wxDV_NO_HEADER | wxDV_SINGLE);
	_modelTreeView->SetMinClientSize(wxSize(300, -1));
    _modelTreeView->SetExpandTopLevelItemsAfterPopulation(true);

	// Single text column
	_modelTreeView->AppendIconTextColumn(_("Model Definition"), _modelColumns.iconAndName.getColumnIndex(),
		wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE);

	// Apply full-text search to the column
	_modelTreeView->AddSearchColumn(_modelColumns.leafName);

	// Connect up the selection changed callback
	_modelTreeView->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &MD5AnimationViewer::_onModelSelChanged, this);

    auto toolbar = new wxutil::ResourceTreeViewToolbar(panel, _modelTreeView);
    panel->GetSizer()->Add(toolbar, 0, wxEXPAND | wxALIGN_LEFT | wxBOTTOM | wxLEFT | wxRIGHT, 6);
    panel->GetSizer()->Add(_modelTreeView, 1, wxEXPAND);

	// Pack into scrolled window and return
	return panel;
}

wxWindow* MD5AnimationViewer::createAnimTreeView(wxWindow* parent)
{
	_animTreeView = wxutil::TreeView::CreateWithModel(parent, _animList.get(), wxDV_SINGLE | wxDV_NO_HEADER);
	_animTreeView->EnableAutoColumnWidthFix(false);

	_animTreeView->SetMinClientSize(wxSize(300, -1));

	// Single text column
	_animTreeView->AppendTextColumn(_("Animation"), _animColumns.name.getColumnIndex(),
		wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE);
	_animTreeView->AppendTextColumn(_("File"), _animColumns.filename.getColumnIndex(),
		wxDATAVIEW_CELL_INERT, wxCOL_WIDTH_AUTOSIZE);

	// Apply full-text search to the column
	_animTreeView->AddSearchColumn(_animColumns.name);

	// Connect up the selection changed callback
	_animTreeView->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &MD5AnimationViewer::_onAnimSelChanged, this);

	return _animTreeView;
}

void MD5AnimationViewer::_onModelSelChanged(wxDataViewEvent& ev)
{
	handleModelSelectionChange();
}

void MD5AnimationViewer::handleModelSelectionChange()
{
	auto modelDef = getSelectedModelDef();

	if (!modelDef)
	{
		_animTreeView->Enable(false);
		return;
	}

	_animTreeView->Enable(true);

	auto modelNode =  GlobalModelCache().getModelNode(modelDef->getMesh());
	_preview->setAnim(md5::IMD5AnimPtr());
	_preview->setModelNode(modelNode);
	
	populateAnimationList();
}

std::string MD5AnimationViewer::getSelectedModel()
{
    return _modelTreeView->GetSelectedDeclName();
}

void MD5AnimationViewer::setSelectedModel(const std::string& model)
{
    _modelTreeView->SetSelectedDeclName(model);
}

std::string MD5AnimationViewer::getSelectedAnim()
{
	auto item = _animTreeView->GetSelection();

	if (!item.IsOk())
	{
		return std::string();
	}

	wxutil::TreeModel::Row row(item, *_animList);
	return static_cast<std::string>(row[_animColumns.name]);
}

void MD5AnimationViewer::setSelectedAnim(const std::string& anim)
{
	if (IsShownOnScreen())
	{
		auto item = _animList->FindString(anim, _animColumns.name);

		if (item.IsOk())
		{
			_animTreeView->Select(item);
			_animTreeView->EnsureVisible(item);
			handleAnimSelectionChange();
		}

		_animToSelect.clear();
		return;
	}

	_animToSelect = anim;
}

IModelDef::Ptr MD5AnimationViewer::getSelectedModelDef()
{
	std::string modelDefName = getSelectedModel();

	if (modelDefName.empty())
	{
		return {};
	}

	return GlobalEntityClassManager().findModel(modelDefName);
}

void MD5AnimationViewer::_onAnimSelChanged(wxDataViewEvent& ev)
{
	handleAnimSelectionChange();
}

void MD5AnimationViewer::handleAnimSelectionChange()
{
	auto modelDef = getSelectedModelDef();

	if (!modelDef) 
	{
		_preview->setAnim(md5::IMD5AnimPtr());
		return;
	}

	wxDataViewItem item = _animTreeView->GetSelection();

	if (!item.IsOk())
	{
		_preview->setAnim(md5::IMD5AnimPtr());
		return;
	}

	wxutil::TreeModel::Row row(item, *_animList);
	std::string filename = row[_animColumns.filename];

	// Assign preview animation
	md5::IMD5AnimPtr anim = GlobalAnimationCache().getAnim(filename);
	_preview->setAnim(anim);
}

void MD5AnimationViewer::populateModelList()
{
    _modelTreeView->Populate(std::make_shared<ThreadedModelDefLoader>(_modelColumns));
}

void MD5AnimationViewer::populateAnimationList()
{
	_animList->Clear();

	auto modelDef = getSelectedModelDef();

	if (!modelDef) return;

	for (const auto& [key, filename] : modelDef->getAnims())
	{
		wxutil::TreeModel::Row row = _animList->AddItem();

		row[_animColumns.name] = key;
		row[_animColumns.filename] = filename;

		row.SendItemAdded();
	}
}

} // namespace
