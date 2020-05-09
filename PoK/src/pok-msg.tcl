# pok-msg.tcl -- poke MI messages

# Copyright (C) 2020 Jose E. Marchesi

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see
# <http://www.gnu.org/licenses/>.

# MI message types

set MI_MSG_TYPE_REQUEST 0
set MI_MSG_TYPE_RESPONSE 1
set MI_MSG_TYPE_EVENT 2

# Request types

set MI_REQ_TYPE_EXIT 0

# Response types

set MI_RESP_TYPE_EXIT 0

# Event types

set MI_EVENT_TYPE_INITIALIZED 0

# pok_msg_make_request REQ_TYPE ARGS...
#
# Create a new MI message of type REQUEST with request type REQ_TYPE
# and request arguments ARGS, which should be a dictionary

proc pok_msg_make_request {req_type args} {
    global MI_MSG_TYPE_REQUEST

    return [dict create \
                poke_mi 0 \
                type $MI_MSG_TYPE_REQUEST \
                data [dict create type $req_type]]
}

# pok_msg_to_json MSG
#
# Return a string with the JSON representation of the given message
# MSG.

proc pok_msg_to_json {msg} {

    ::json::write indented 0
    ::json::write object {*}[dict map {k v} $msg {
        if {[string equal $k data]} {
            set v [pok_msg_to_json $v]
        } elseif {[string equal $k type]
                  || [string equal $k poke_mi]} {
            set v $v
        } else {
            set v [::json::write string $v]
        }
    }]
}
