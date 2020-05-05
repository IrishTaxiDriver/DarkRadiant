#pragma once

#include <utility>

#include "ieventmanager.h"
#include "ifilter.h"

namespace filters
{

class XMLFilter;

/**
 * An object responsible for managing the commands and events 
 * bound to a single XMLFilter object.
 */
class XmlFilterEventAdapter
{
private:
    XMLFilter& _filter;

#if 0
    std::pair<std::string, IEventPtr> _toggle;
#endif
    std::string _toggleCmdName;
    std::string _selectByFilterCmd;
    std::string _deselectByFilterCmd;

public:
    typedef std::shared_ptr<XmlFilterEventAdapter> Ptr;

    XmlFilterEventAdapter(XMLFilter& filter);

    ~XmlFilterEventAdapter();

    // Synchronisation routine to notify this class once the filter has been activated
    void setFilterState(bool isActive);

    // Post-filter-rename event, to be invoked by the FilterSystem after a rename operation
    void onEventNameChanged();

private:
    // The command target
    void toggle(bool newState);

    void createSelectDeselectEvents();
    void removeSelectDeselectEvents();
    void createToggleCommand();
};

}
