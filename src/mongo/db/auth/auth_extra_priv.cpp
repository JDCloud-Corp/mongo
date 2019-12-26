/**
*    Copyright (C) 2019 JD.com, Inc., JD Cloud.
*
*    This program is free software: you can redistribute it and/or  modify
*    it under the terms of the GNU Affero General Public License, version 3,
*    as published by the Free Software Foundation.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Affero General Public License for more details.
*
*    You should have received a copy of the GNU Affero General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kAccessControl
#include "mongo/util/log.h"
#include "auth_extra_priv.h"
namespace mongo {
namespace {
ActionSet& operator<<(ActionSet& target, ActionType source) {
    target.addAction(source);
    return target;
}
}  // namepsace

bool authPrivExtra(ActionSet& set)
{
    static ActionSet extraRoleActions;
    if (extraRoleActions.empty()) {
        // hostManager role actions that target the database resource
        extraRoleActions << ActionType::killCursors << ActionType::repairDatabase;

        // clusterManager role actions that target the cluster resource
        extraRoleActions
                << ActionType::appendOplogNote     // backup gets this also
                << ActionType::applicationMessage  // hostManager gets this also
                << ActionType::replSetConfigure
                << ActionType::replSetGetConfig                          // clusterMonitor gets this also
                << ActionType::replSetGetStatus                          // clusterMonitor gets this also
                << ActionType::replSetStateChange << ActionType::resync  // hostManager gets this also
                << ActionType::addShard << ActionType::removeShard
                << ActionType::listShards         // clusterMonitor gets this also
                << ActionType::flushRouterConfig  // hostManager gets this also
                << ActionType::cleanupOrphaned;

        extraRoleActions << ActionType::splitChunk << ActionType::moveChunk
                         << ActionType::enableSharding << ActionType::splitVector;
    }
    set.removeAllActionsFromSet(extraRoleActions);
    if (set.empty())
        return true;
    return false;
}
}
