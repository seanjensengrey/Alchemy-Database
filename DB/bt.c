/* B-tree Implementation.
 *
 * Implements in memory b-tree tables with insert/del/replace/find/ ops

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <strings.h>

#include "redis.h" /* defines REDIS_BTREE */

#include "btree.h"
#include "bt.h"
#include "index.h"
#include "query.h"
#include "stream.h"
#include "common.h"

extern r_tbl_t *Tbl;
extern r_ind_t *Index;

#define CREATE_OBT(kt, ks, bf, cmp)                                         \
    bts_t bts;           bts.ktype = kt;                 bts.btype = btype; \
    bts.ksize = ks;      bts.bflag = bf;                 bts.num   = num;   \
    return bt_create(cmp, TRANS_ONE, &bts);

bt *createUUBT(int num, uchar btype) {                //printf("createUUBT\n");
    CREATE_OBT(COL_TYPE_INT, UU_SIZE, BTFLAG_UINT_UINT, uuCmp);
}
bt *createULBT(int num, uchar btype) {                //printf("createULBT\n");
    CREATE_OBT(COL_TYPE_INT, UL_SIZE, BTFLAG_UINT_ULONG, ulCmp);
}
bt *createLUBT(int num, uchar btype) {                //printf("createLUBT\n");
    CREATE_OBT(COL_TYPE_LONG, LU_SIZE, BTFLAG_ULONG_UINT, luCmp);
}
bt *createLLBT(int num, uchar btype) {                //printf("createLLBT\n");
    CREATE_OBT(COL_TYPE_LONG, LL_SIZE, BTFLAG_ULONG_ULONG, llCmp);
}
bt *createUXBT(int num, uchar btype) {                //printf("createUXBT\n");
    CREATE_OBT(COL_TYPE_INT, UX_SIZE, BTFLAG_UINT_U128, uxCmp);
}
bt *createXUBT(int num, uchar btype) {                //printf("createXUBT\n");
    CREATE_OBT(COL_TYPE_U128, XU_SIZE, BTFLAG_U128_UINT, xuCmp);
}
bt *createLXBT(int num, uchar btype) {                //printf("createLXBT\n");
    CREATE_OBT(COL_TYPE_LONG, LX_SIZE, BTFLAG_ULONG_U128, lxCmp);
}
bt *createXLBT(int num, uchar btype) {                //printf("createXLBT\n");
    CREATE_OBT(COL_TYPE_U128, XL_SIZE, BTFLAG_U128_ULONG, xlCmp);
}
bt *createXXBT(int num, uchar btype) {                //printf("createXXBT\n");
    CREATE_OBT(COL_TYPE_U128, XX_SIZE, BTFLAG_U128_U128, xxCmp);
}
static bt *createOBT(uchar ktype, uchar vtype, int tmatch, uchar btype) {
    if        (C_IS_I(ktype)) {
        if (C_IS_I(vtype)) return createUUBT(tmatch, btype);
        if (C_IS_L(vtype)) return createULBT(tmatch, btype);
        if (C_IS_X(vtype)) return createUXBT(tmatch, btype);
    } else if (C_IS_L(ktype)) {
        if (C_IS_I(vtype)) return createLUBT(tmatch, btype);
        if (C_IS_L(vtype)) return createLLBT(tmatch, btype);
        if (C_IS_X(vtype)) return createLXBT(tmatch, btype);
    } else if (C_IS_X(ktype)) {
        if (C_IS_I(vtype)) return createXUBT(tmatch, btype);
        if (C_IS_L(vtype)) return createXLBT(tmatch, btype);
        if (C_IS_X(vtype)) return createXXBT(tmatch, btype);
    }
    return NULL;
}
#define ASSIGN_CMP(ktype)   C_IS_I(ktype) ? btIntCmp   : \
                          ( C_IS_L(ktype) ? btLongCmp  : \
                          ( C_IS_F(ktype) ? btFloatCmp : \
                          /* STRING */      btTextCmp))

bt *createDBT(uchar ktype, int tmatch) {
    r_tbl_t *rt = &Tbl[tmatch];
    if (rt->col_count == 2) {
        bt *obtr = createOBT(ktype, rt->col[1].type, tmatch, BTREE_TABLE);
        if (obtr) return obtr;
    }
    bts_t bts;
    bts.ktype = ktype;       bts.btype = BTREE_TABLE; bts.ksize = VOIDSIZE;
    bts.bflag = BTFLAG_NONE; bts.num   = tmatch; 
    return bt_create(ASSIGN_CMP(ktype), TRANS_ONE, &bts);
}
bt *createIBT(uchar ktype, int imatch, uchar btype) {
    bt_cmp_t cmp; bts_t bts;
    bts.ktype = ktype; bts.btype = btype; bts.num = imatch;
    if        C_IS_I(ktype) { /* NOTE: under the covers: ULBT */
        bts.ksize = UL_SIZE; cmp = ulCmp;
        bts.bflag = BTFLAG_UINT_ULONG  + BTFLAG_UINT_INDEX;
    } else if C_IS_L(ktype) { /* NOTE: under the covers: LLBT */
        bts.ksize = LL_SIZE; cmp = llCmp;
        bts.bflag = BTFLAG_ULONG_ULONG + BTFLAG_ULONG_INDEX;
    } else if C_IS_X(ktype) { /* NOTE: under the covers: XLBT */
        bts.ksize = XL_SIZE; cmp = xlCmp;
        bts.bflag = BTFLAG_U128_ULONG  + BTFLAG_U128_INDEX;
    } else {                  /* STRING or FLOAT */
        bts.ksize = VOIDSIZE; cmp = ASSIGN_CMP(ktype);
        bts.bflag = BTFLAG_NONE;
    }
    return bt_create(cmp, TRANS_ONE, &bts);
}
bt *createMCI_MIDBT(uchar ktype, int imatch) {
    return createIBT(ktype, imatch, BTREE_MCI_MID);
}
bt *createMCIndexBT(list *clist, int imatch) {
    listNode *ln    = listFirst(clist);
    r_tbl_t  *rt    = &Tbl[Index[imatch].table];
    uchar     ktype = rt->col[(int)(long)ln->value].type;
    return createIBT(ktype, imatch, BTREE_MCI);
}
bt *createIndexNode(uchar ktype, uchar obctype) {                /* INODE_BT */
    if (obctype != COL_TYPE_NONE) {
        bt *btr       =  createOBT(obctype, ktype, -1, BTREE_INODE);
        btr->s.bflag |= BTFLAG_OBC; // will fail INODE(btr) -> OBC is different
        // NOTE: BTFLAG_*_INDEX used as the RAW [VALUE] is pulled out in
        //       parseStream() and passed to nodeBT_Op()
        //       [UU,UL,LU,LL] are passed as [KEY,VALUE] to getRawCol()
        btr->s.bflag |= (C_IS_I(obctype)  ? BTFLAG_UINT_INDEX  :
                        (C_IS_L(ktype)    ? BTFLAG_ULONG_INDEX :
                      /* C_IS_X(ktype) */   BTFLAG_U128_INDEX));
        return btr;
    }
    bt_cmp_t cmp; bts_t bts;
    bts.ktype = ktype; bts.btype = BTREE_INODE; bts.num   = -1;
    if        (C_IS_I(ktype)) {
        cmp = uintCmp;           bts.ksize = UINTSIZE;  bts.bflag = BTFLAG_NONE;
    } else if (C_IS_L(ktype)) {
        cmp = ulongCmp;          bts.ksize = ULONGSIZE; bts.bflag = BTFLAG_NONE;
    } else if (C_IS_X(ktype)) {
        cmp = u128Cmp;           bts.ksize = U128SIZE;  bts.bflag = BTFLAG_NONE;
    } else {
        cmp = ASSIGN_CMP(ktype); bts.ksize = VOIDSIZE;  bts.bflag = BTFLAG_NONE;
    }
    return bt_create(cmp, TRANS_ONE, &bts);
}
bt *createIndexBT(uchar ktype, int imatch) {
    return createIBT(ktype, imatch, BTREE_INDEX);
}

/* ABSTRACT-BTREE ABSTRACT-BTREE ABSTRACT-BTREE ABSTRACT-BTREE ABSTRACT-BTREE */
#define DEBUG_REP_NSIZE         printf("osize: %d nsize: %d\n", osize, nsize);
#define DEBUG_REP_SAMESIZE      printf("abt_replace: SAME_SIZE\n");
#define DEBUG_REP_REALLOC       printf("o2: %p o: %p\n", o2stream, *ostream);
#define DEBUG_REP_REALLOC_OVRWR printf("abt_replace: REALLOC OVERWRITE\n");
#define DEBUG_REP_RELOC_NEW     printf("abt_replace: REALLOC -> new \n");

#define DECLARE_BT_KEY                                                     \
    bool  med; uint32 ksize;                                               \
    char *btkey = createBTKey(akey, &med, &ksize, btr);  /* FREE ME 026 */ \
    if (!btkey) return 0;
    
static int abt_replace(bt *btr, aobj *akey, void *val) {
    uint32 ssize;
    DECLARE_BT_KEY
    uchar  *nstream = createStream(btr, val, btkey, ksize, &ssize);
    if NORM_BT(btr) {
        uchar  **ostream = (uchar **)bt_find_loc(btr, btkey);
        destroyBTKey(btkey, med);                        /* FREED 026 */
        uint32   osize   = getStreamMallocSize(btr, *ostream);
        uint32   nsize   = getStreamMallocSize(btr,  nstream); //DEBUG_REP_NSIZE
        if (osize == nsize) { /* if ROW size doesnt change, just overwrite */
            memcpy(*ostream, nstream, osize);             //DEBUG_REP_SAMESIZE
            destroyStream(btr, nstream);
        } else {              /* try to realloc (may re-use memory -> WIN) */
            bt_decr_dsize(btr, osize); bt_incr_dsize(btr, nsize);
            void *o2stream = realloc(*ostream, nsize);      //DEBUG_REP_REALLOC
            if (o2stream == *ostream) { /* realloc overwrite -> WIN */
                memcpy(*ostream, nstream, nsize);     //DEBUG_REP_REALLOC_OVRWR
                destroyStream(btr, nstream);
            } else {          /* new pointer, copy nstream, replace in BT */
                *ostream = nstream;/* REPLACE ptr in BT */ //DEBUG_REP_RELOC_NEW
            }
        }
    } else {
        uchar *dstream = bt_replace(btr, btkey, nstream);
        destroyBTKey(btkey, med);                        /* FREED 026 */
        destroyStream(btr, dstream);
    }
    return ssize;
}
static void *abt_find(bt *btr, aobj *akey) {
    DECLARE_BT_KEY
    uchar *stream = bt_find(btr, btkey);
    destroyBTKey(btkey, med);                            /* FREED 026 */
    return parseStream(stream, btr);
}
static bool abt_del(bt *btr, aobj *akey) {
    DECLARE_BT_KEY
    uchar *stream = bt_delete(btr, btkey);               /* FREED 028 */
    destroyBTKey(btkey, med);                            /* FREED 026 */
    return destroyStream(btr, stream);                   /* DESTROYED 027 */
}
static uint32 abt_insert(bt *btr, aobj *akey, void *val) {
    if (btr->numkeys == TRANS_ONE_MAX) btr = abt_resize(btr, TRANS_TWO);
    uint32 ssize;
    DECLARE_BT_KEY
    char   *stream = createStream(btr, val, btkey, ksize, &ssize); /*DEST 027*/
    destroyBTKey(btkey, med);                            /* FREED 026 */
    bt_insert(btr, stream);                              /* FREE ME 028 */
    return ssize;
}
bt *abt_resize(bt *obtr, uchar trans) {              // printf("abt_resize\n");
     bts_t bts;
     memcpy(&bts, &obtr->s, sizeof(bts_t)); /* copy flags */
     bt *nbtr    = bt_create(obtr->cmp, trans, &bts);
     nbtr->dsize = obtr->dsize;
    if (obtr->root) {
        bt_to_bt_insert(nbtr, obtr, obtr->root); /* 1.) copy from old to new */
        bt_release(obtr, obtr->root);            /* 2.) release old */
        memcpy(obtr, nbtr, sizeof(bt));          /* 3.) overwrite old w/ new */
        free(nbtr);                              /* 4.) free new */
    } //bt_dump_info(obtr, obtr->ktype);
    return obtr;
}

/* DATA DATA DATA DATA DATA DATA DATA DATA DATA DATA DATA DATA DATA DATA */
int   btAdd(bt *btr, aobj *apk, void *val) { return abt_insert( btr, apk, val);}
void *btFind(bt *btr, aobj *apk) {           return abt_find(   btr, apk); }
int   btReplace(bt *btr, aobj *apk, void *val) {
                                             return abt_replace(btr, apk, val);}
int   btDelete( bt *btr, aobj *apk) {        return abt_del(    btr, apk); }

/* INDEX INDEX INDEX INDEX INDEX INDEX INDEX INDEX INDEX INDEX INDEX INDEX */
void btIndAdd(bt *ibtr, aobj *akey, bt *nbtr) { abt_insert(ibtr, akey, nbtr); }
bt *btIndFind(bt *ibtr, aobj *akey) {    return abt_find(  ibtr, akey); }
int btIndDelete(bt *ibtr, aobj *akey) {
    abt_del(ibtr, akey);
    return ibtr->numkeys;
}

/* INDEX_NODE INDEX_NODE INDEX_NODE INDEX_NODE INDEX_NODE INDEX_NODE */
#define DEBUG_IADD_OBC                                       \
    printf("btIndNodeOBCAdd: apk : "); dumpAobj(printf, apk);  \
    printf("btIndNodeOBCAdd: ocol: "); dumpAobj(printf, ocol);

void *btIndNodeFind  (bt *nbtr, aobj *apk) { return abt_find(nbtr, apk); }
void  btIndNodeAdd   (bt *nbtr, aobj *apk) { abt_insert(nbtr, apk, NULL); }
int   btIndNodeDelete(bt *nbtr, aobj *apk) {
    abt_del(nbtr, apk); return nbtr->numkeys;
}
bool  btIndNodeOBCAdd(cli *c, bt *nbtr, aobj *apk, aobj *ocol) {//DEBUG_IADD_OBC
    if (abt_find(nbtr, ocol)) {
        if (c) addReply(c, shared.obindexviol); return 0;
    }
    if      C_IS_I(apk->type) abt_insert(nbtr, ocol, VOIDINT apk->i);
    else /* C_IS_L */         abt_insert(nbtr, ocol, (void *)apk->l);
    return 1;
}
int   btIndNodeOBCDelete(bt *nbtr, aobj *ocol) {
    abt_del(nbtr, ocol); return nbtr->numkeys;
}
