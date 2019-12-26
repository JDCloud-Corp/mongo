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
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/file.h>
#include <string>
#include "auth_extra_priv.h"
#include "mongo/util/log.h"
namespace mongo {
namespace {
static volatile sig_atomic_t is_read_only = false;
static volatile sig_atomic_t is_no_read_write  = false;
ActionSet& operator<<(ActionSet& target, ActionType source) {
    target.addAction(source);
    return target;
}
}  // namespace

bool authUserNoWrite(ActionSet& set)
{
    if (is_read_only) {
        static ActionSet writeRoleActions;  // no need to initialize everytime
        if (writeRoleActions.empty()) {
            writeRoleActions << ActionType::convertToCapped   // db admin gets this also
                             << ActionType::createCollection  // db admin gets this also
                             << ActionType::dropCollection << ActionType::dropIndex
                             << ActionType::emptycapped << ActionType::createIndex << ActionType::insert
                             << ActionType::remove
                             << ActionType::renameCollectionSameDB  // db admin gets this also
                             << ActionType::update;
        }
        set.removeAllActionsFromSet(writeRoleActions);
        if (set.empty())
            return true;
    }
    return false;
}

bool authUserNoReadWrite(ActionSet& set)
{
    if (is_no_read_write) {
        static ActionSet writeReadRoleActions;  // no need to initialize everytime
        if (writeReadRoleActions.empty()) {
            // make sure the action set is the same with role_graph_builtin_roles.cpp
            writeReadRoleActions << ActionType::convertToCapped   // db admin gets this also
                                 << ActionType::createCollection  // db admin gets this also
                                 << ActionType::dropCollection << ActionType::dropIndex
                                 << ActionType::emptycapped << ActionType::createIndex << ActionType::insert
                                 << ActionType::remove
                                 << ActionType::renameCollectionSameDB  // db admin gets this also
                                 << ActionType::update
                    // below are read action type
                                 << ActionType::collStats << ActionType::dbHash << ActionType::dbStats
                                 << ActionType::find << ActionType::killCursors << ActionType::listCollections
                                 << ActionType::listIndexes << ActionType::planCacheRead;
        }
        set.removeAllActionsFromSet(writeReadRoleActions);
        if (set.empty())
            return true;
    }
    return false;
}

static inline void _readWriteHandler(const char *fname, volatile sig_atomic_t &flag)
{
#define EVENT_BUFFER_SIZE 33
    FILE *fd;
    char *mnt = NULL;
    std::string path;
    char buffer[EVENT_BUFFER_SIZE];

    if ((mnt = getenv("MOUNTDIR")) != NULL) {
        path = mnt + std::string("/var/") + fname;
    } else {
        path = std::string("./") + fname;
    }
    
    if ((fd = fopen(path.c_str(), "r")) == NULL) {
        LOG(1) << "Open file " << path << " failed!";
        return;
    }
    // we use flock here not POSIX lock, becase I can not found shell command support POSIX lock.
    if (flock(fileno(fd), LOCK_EX) != 0) {
        fclose(fd);
        LOG(1) << "Acquire file lock failed!";
        return;
    }

    if (fgets(buffer, EVENT_BUFFER_SIZE, fd) == NULL) {
        LOG(1) << "fgets failed!";
        flock(fileno(fd), LOCK_UN);
        fclose(fd);
        return;
    }
    
    if (strlen(buffer) != EVENT_BUFFER_SIZE-1 || !isprint(buffer[EVENT_BUFFER_SIZE-2])) {
        LOG(1) << "read " << EVENT_BUFFER_SIZE-1 << " event byte failed!";
        flock(fileno(fd), LOCK_UN);
        fclose(fd);
        return;
    }
    
    LOG(1) << "receive read only event string: " << buffer;
    int i = 0;
    for (i = 0; i < EVENT_BUFFER_SIZE-1; i++) {
        if ('1' == buffer[i]) {
            flag = true;
            break;
        }
    }
    if (EVENT_BUFFER_SIZE-1 == i) {
        flag = false;
    }
    flock(fileno(fd), LOCK_UN);
    fclose(fd);
}

static void handleExternalReadOnlySignal()
{
    const char * fname = "read_only";
    _readWriteHandler(fname, is_read_only);
}

static void handleExternalReadWriteSignal()
{
    const char * fname = "no_read_write";
    _readWriteHandler(fname, is_no_read_write);
}

void handleUserDefinedSignal()
{
	handleExternalReadOnlySignal();
	handleExternalReadWriteSignal();
}
}

