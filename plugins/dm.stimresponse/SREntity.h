#ifndef SRENTITY_H_
#define SRENTITY_H_

#include <vector>
#include <string>
#include <gtk/gtktreemodel.h>
#include <boost/shared_ptr.hpp>

#include "StimResponse.h"
#include "StimTypes.h"
#include "ResponseScript.h"

// Forward declaration
class Entity;
typedef struct _GtkListStore GtkListStore;

	namespace {
		const std::string ICON_STIM = "sr_stim.png";
		const std::string ICON_RESPONSE = "sr_response.png";
		
		enum {
			INDEX_COL,
			CLASS_COL,
			CAPTION_COL,
			ICON_COL,
			INHERIT_COL,
			IDSTR_COL,
			NUM_COLS
		};
		
		enum {
			SCR_ID_COL,
			SCR_CAPTION_COL,
			SCR_NAME_COL,
			SCR_ICON_COL,
			SCR_SCRIPT_COL,
			SCR_NUM_COLS
		};
	}

/** greebo: This is the representation of an entity holding S/R keys.
 * 			Use the load() and save() methods to load/save the spawnargs.
 * 			
 * 			The operator cast to GtkListStore* is available to pack
 * 			the data into a GtkTreeView.
 */
class SREntity
{
public:
	// These are the possible key names
	typedef std::vector<std::string> KeyList;
	
	// These are the int-indexed Stims/Responses belonging to an entity
	typedef std::map<int, StimResponse> StimResponseMap;
	
private:
	// The local lists of S/R and possible keys
	StimResponseMap _list;
	KeyList _keys;
	
	// The liststore representation
	GtkListStore* _listStore;
	
	// A collection of warnings regarding the parsing of the spawnargs
	std::string _warnings;
	
	// The helper class managing the various stim types
	StimTypes _stimTypes;
	
	// An empty stim
	StimResponse _emptyStimResponse;
	
	// Helper object managing the scripts 
	ResponseScripts _scripts;
	GtkListStore* _scriptStore;

public:
	SREntity(Entity* source);

	void load(Entity* source);
	void save(Entity* target);
	
	/** greebo: Retrieves the reference to the StimResponse object
	 * 			having the given integer <id>.
	 * 
	 * @returns: The ref to the StimResponse or an empty StimResponse object,
	 * 			 if the id was not found.
	 */
	StimResponse& get(int id);
	
	/** greebo: Adds a new StimResponse and returns the id of the new object.
	 * 			The ListStore is NOT updated with this call to allow setting of  
	 * 			the properties before refreshing the treeview. 
	 */
	int add();
	
	/** greebo: Operator cast to a GtkListStore*. Use this to add the liststore
	 * 			to a treeview or a combobox.
	 */
	operator GtkListStore* ();
	
	/** greebo: Sets the <key> of the SR with the given <id> to <value>
	 */
	void setProperty(int id, const std::string& key, const std::string& value);
	
	/** greebo: Updates the ListStore according to the 
	 * 			values of the current StimResponseMap <_list>
	 */
	void updateListStore();
	
	/** greebo: Helper to load the possible key names
	 * 			from the registry into the _keys list.
	 */
	void loadKeys();
	
	/** greebo: Returns the treeIter pointing to the row containing the 
	 * 			StimResponse with the given <id>
	 */
	GtkTreeIter getIterForId(int id);
	
private:
	/** greebo: Write the values of the passed StimResponse to the 
	 * 			GtkListStore using the passed GtkTreeIter.
	 * 			The ID stays untouched. 
	 * 
	 * @iter: The TreeIter pointing at the row where the data should be inserted
	 * @sr: the StimResponse object containing the source data
	 */
	void writeToListStore(GtkTreeIter* iter, StimResponse& sr);
	
	/** greebo: Write the values of the passed ResponseScript to the 
	 * 			GtkListStore using the passed GtkTreeIter. 
	 * 
	 * @iter: The TreeIter pointing at the row where the data should be inserted
	 * @script: the ResponseScript object containing the source data
	 */
	void writeToScriptStore(GtkTreeIter* iter, ResponseScript& script);

	// Returns the highest currently assigned id
	int getHighestId();
	
	// Returns the highest Stim/Response index number 
	int getHighestIndex();
};

typedef boost::shared_ptr<SREntity> SREntityPtr; 

#endif /*SRENTITY_H_*/
