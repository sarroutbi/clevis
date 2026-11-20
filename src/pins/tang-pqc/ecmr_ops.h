// vim: set tabstop=8 shiftwidth=4 softtabstop=4 expandtab smarttab colorcolumn=80:
//
// Copyright (c) 2026 Red Hat, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include "jose_types.h"

namespace clevis {

class EcmrOps {
public:
    // The blinded value is public (it is sent to the server), but
    // the unblind intermediate and the recovered share are secret:
    // hand everything out cleansed so no exchange result is freed
    // without wiping.
    static CleansedJsonPtr blind(
        const json_t* clientPub, const json_t* ephemeral);
    static CleansedJsonPtr unblind(
        const json_t* response,
        const json_t* ephemeral,
        const json_t* serverPub);
};

} // namespace clevis
