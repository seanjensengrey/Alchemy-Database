/*
 * This file implements sql parsing for ALCHEMY_DATABASE WhereClauses
 *

AGPL License

Copyright (c) 2011 Russell Sullivan <jaksprats AT gmail DOT com>
ALL RIGHTS RESERVED 

   This file is part of ALCHEMY_DATABASE

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __REDISQL_WHERECLAUSE__H
#define __REDISQL_WHERECLAUSE__H

#include "redis.h"

#include "query.h"
#include "common.h"

#define SQL_ERR_LKP         0 
#define SQL_SINGLE_LKP      1
#define SQL_RANGE_LKP       2
#define SQL_IN_LKP          3
#define SQL_SINGLE_FK_LKP   4

#define SQL_SELECT     0
#define SQL_DELETE     1
#define SQL_UPDATE     2

#define PARSE_OK       0
#define PARSE_GEN_ERR  1
#define PARSE_NEST_ERR 2

bool  parseWCEnd(cli *c, char *token, cswc_t *w, wob_t *wb);
  
uchar parseWC(cli *c, cswc_t *w, wob_t *wb, jb_t *jb);
void  parseWCReply(cli *c, cswc_t *w, wob_t *wb, uchar sop);

bool  parseJoinReply(cli *c, jb_t *jb, char *clist, char *tlist, char *wc);
bool  executeJoin(cli *c, jb_t *jb);
void  joinReply(cli *c);

#endif /* __REDISQL_WHERECLAUSE__H */