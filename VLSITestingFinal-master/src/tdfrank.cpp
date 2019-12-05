#include "atpg.h"
#include <iostream>
#include <climits>
#include <cassert>

using namespace std;

// last 2 bit: [0:1st 1:2nd timeframe][0 1:complement]
// pointer operation
static inline size_t ptrNotCond(void* p, bool c) { return ((size_t)p ^ (size_t)c); }
static inline size_t ptrNotCond(size_t p, bool c) { return ((size_t)p ^ (size_t)c); }
static inline size_t ptrRegular(size_t p) { return ((size_t)p & ~(size_t)1); }
static inline bool ptrIsCompl(size_t p) { return bool(size_t(p) & size_t(1)); }
static inline size_t ptrTimeShift(size_t p) { return p ^ size_t(2); }

// set operation
static inline void setUnion(set<size_t>& s1, set<size_t> s2) { for(size_t _ptr: s2) s1.insert(_ptr); }
static set<size_t> setTimeShift(const set<size_t>& s)
{
    set<size_t> ret;
    for(size_t ptr: s) ret.insert(ptrTimeShift(ptr));
    return ret;
}
static inline void setUnionTimeShift(set<size_t>& s1, set<size_t> s2) { for(size_t _ptr: s2) s1.insert(ptrTimeShift(_ptr)); }


void ATPG::computeRC()
{
    for(wptr w: sort_wlist) {
        const nptr n = w->inode[0];
        switch(n->type) {
        case INPUT:
            w->RC1.insert(ptrNotCond(n, false));
            w->RC0.insert(ptrNotCond(n, true));
            break;
        case BUF:
            w->RC1 = n->iwire[0]->RC1;
            w->RC0 = n->iwire[0]->RC0;
            break;
        case NOT:
            w->RC1 = n->iwire[0]->RC0;
            w->RC0 = n->iwire[0]->RC1;
            break;
        case AND: case OR:
            w->RC1 = n->iwire[0]->RC1;
            w->RC0 = n->iwire[0]->RC0;
            for(int i=1, k=n->iwire.size(); i<k; ++i) {
                wptr _w = n->iwire[i];
                if(n->type == AND) {
                    setUnion(w->RC1, _w->RC1);
                    if(_w->RC0.size() < w->RC0.size()) w->RC0 = _w->RC0;
                } else {
                    setUnion(w->RC0, _w->RC0);
                    if(_w->RC1.size() < w->RC1.size()) w->RC1 = _w->RC1;
                }
            }
            break;
        case NAND: case NOR:
            w->RC1 = n->iwire[0]->RC0;
            w->RC0 = n->iwire[0]->RC1;
            for(int i=1, k=n->iwire.size(); i<k; ++i) {
                wptr _w = n->iwire[i];
                if(n->type == NAND) {
                    setUnion(w->RC0, _w->RC1);
                    if(_w->RC0.size() < w->RC1.size()) w->RC1 = _w->RC0;
                } else {
                    setUnion(w->RC1, _w->RC0);
                    if(_w->RC1.size() < w->RC0.size()) w->RC0 = _w->RC1;
                }
            }
            break;
        case XOR: case EQV:
            set<size_t> P1, P2, P3, P4, rc[2];
            for(int i=0, k=n->iwire.size(); i<k; ++i) {
                wptr _w = n->iwire[i];
                rc[1] = _w->RC1;
                rc[0] = _w->RC0;
                setUnion(P1, rc[(i+1)%2]);
                setUnion(P2, rc[i%2]);
                setUnion(P3, rc[1]);
                setUnion(P4, rc[0]);
            }
            if(n->iwire.size()%2 == 0) {
                if(n->type == XOR) {
                    w->RC1 = P1.size()<P2.size() ? P1 : P2;
                    w->RC0 = P3.size()<P4.size() ? P3 : P4;
                } else {
                    w->RC0 = P1.size()<P2.size() ? P1 : P2;
                    w->RC1 = P3.size()<P4.size() ? P3 : P4;
                }
            } else {
                if(n->type == XOR) {
                    w->RC1 = P1.size()<P3.size() ? P1 : P3;
                    w->RC0 = P2.size()<P4.size() ? P2 : P4;
                } else {
                    w->RC0 = P1.size()<P3.size() ? P1 : P3;
                    w->RC1 = P2.size()<P4.size() ? P2 : P4;
                }
            }
            break;
        }
        /* debug message
        cout << "=================================\n";
        cout << "gate type " << n->type << "\n";
        cout << "RC1: ";
        for(size_t ptr: w->RC1) cout << ptr << " ";
        cout << "\nRC0: ";
        for(size_t ptr: w->RC0) cout << ptr << " ";
        cout << "\n";
        */
    }
}


map<pair<size_t, short>, set<size_t> > ATPG::computeRO()
{
    map<pair<size_t, short>, set<size_t> > ROMap;
    for(auto it=sort_wlist.crbegin(); it!=sort_wlist.rend(); ++it) {
        wptr w = *it;
        int minROsize = INT_MAX;
        set<size_t> minRO;
        for(nptr n: w->onode) {
            size_t k = size_t(n);
            short idx = 0;
            set<size_t> RO;
            
            if(n->type == OUTPUT) {
                ROMap[make_pair(k, 0)] = RO;
                ROMap[make_pair(k+1, 0)] = RO;
            } else {
                RO = ROMap[make_pair(k+1, 0)];
                switch(n->type) {
                case NOT: case BUF:
                    idx = 0;
                    break;
                case AND: case NAND:
                    for(int i=0; i<n->iwire.size(); ++i) {
                        if(n->iwire[i] == w) idx = i;
                        else setUnion(RO, n->iwire[i]->RC1);
                    }
                    break;
                case OR: case NOR:
                    for(int i=0; i<n->iwire.size(); ++i) {
                        if(n->iwire[i] == w) idx = i;
                        else setUnion(RO, n->iwire[i]->RC0);
                    }
                    break;
                case XOR: case EQV:
                    for(int i=0; i<n->iwire.size(); ++i) {
                        if(n->iwire[i] == w) idx = i;
                        else setUnion(RO, n->iwire[i]->RC0.size()<n->iwire[i]->RC1.size()?n->iwire[i]->RC0:n->iwire[i]->RC1);
                    }
                    break;
                }
                ROMap[make_pair(k, idx)] = RO;
            }
            
            if(RO.size() < minROsize) {
                minRO = RO;
                minROsize = RO.size();
            }
        }
        nptr n = w->inode[0];
        ROMap[make_pair(size_t(n)+1, 0)] = minRO;
    }
    return ROMap;
}

map<size_t, size_t> ATPG::buildLOSRelation(bool reverse)
{
    map<size_t, size_t> ret;
    for(int i=0, n=cktin.size(); i<n; ++i) {
        wptr w = cktin[i];
        size_t ptr = size_t(w->inode[0]);
        if(reverse) {
            ptr = ptrTimeShift(ptr);
            if(i == 0) ret[ptr] = 0;
            else ret[ptr] = size_t(cktin[i-1]->inode[0]);
        } else {
            if(i == n-1) ret[ptr] = 0;
            else ret[ptr] = ptrTimeShift(size_t(cktin[i+1]->inode[0]));
        }
    }
    return ret;
}

set<size_t> ATPG::getLOSCstr(set<size_t>& s, map<size_t, size_t>& m)
{
    set<size_t> ret;
    for(auto ptr: s) {
        size_t n = m[ptrRegular(ptr)];
        if(n) ret.insert(ptrNotCond(n, ptrIsCompl(ptr)));
    }
    return ret;
}

void ATPG::computeDetAbility()
{
    computeRC();
    // map (nptr+io, index) -> RO
    map<pair<size_t, short>, set<size_t> > ROMap = computeRO();
    
    map<size_t, size_t> PIMap = buildLOSRelation(false);
    map<size_t, size_t> rPIMap = buildLOSRelation(true);
    set<size_t> v2;
    for(fptr f: flist_undetect) {
        set<size_t> da;
        switch(f->fault_type) {
        case STF: // 1->0
            // v1
            setUnion(da, sort_wlist[f->to_swlist]->RC1);
            setUnion(da, getLOSCstr(sort_wlist[f->to_swlist]->RC1, PIMap));
            // v2
            v2 = setTimeShift(sort_wlist[f->to_swlist]->RC0);
            setUnion(da, v2);
            setUnion(da, getLOSCstr(v2, rPIMap));
            break;
        case STR: // 0->1
            // v1
            setUnion(da, sort_wlist[f->to_swlist]->RC0);
            setUnion(da, getLOSCstr(sort_wlist[f->to_swlist]->RC0, PIMap));
            // v2
            v2 = setTimeShift(sort_wlist[f->to_swlist]->RC1);
            setUnion(da, v2);
            setUnion(da, getLOSCstr(v2, rPIMap));
            break;
        }
        
        set<size_t> RO = setTimeShift(ROMap[make_pair(size_t(f->node)+size_t(f->io), f->index)]);
        setUnion(da, RO);
        setUnion(da, getLOSCstr(RO, rPIMap));
        f->detAbility = da;
    }
    //ROMap.clear();
    
    // free RC
    for(wptr w: sort_wlist) {
        w->RC1.clear();
        w->RC0.clear();
    }
}

static int countConflicts(set<size_t> s)
{
    int cnt = 0;
    set<size_t> tmp;
    for(size_t ptr: s) {
        size_t rptr = ptrRegular(ptr);
        cnt += tmp.count(rptr);
        tmp.insert(rptr);
    }
    return cnt;
}

void ATPG::tdfReRank(forward_list<ATPG::fptr>& flist, const set<size_t>& cstr, const int& maxLen, const bool& hard_first)
{
    /* faults will be sorted by #(da conflicts), da.size()*/
    class comp {
    public:
        comp(set<size_t> x, bool hard_first) {
            this->s = x;
            this->hard_first = hard_first;
        }
        bool operator () (fptr x, fptr y) {
            bool ret = compare(x, y);
            if(hard_first) return !ret;
            return ret;
        }

    private:
        bool compare(fptr x, fptr y) {
            set<size_t> i = x->detAbility;
            set<size_t> j = y->detAbility;
            setUnion(i, s);
            setUnion(j, s);
            int c1 = countConflicts(i);
            int c2 = countConflicts(j);
            if(c1 != c2) return c1 < c2;
            return i.size() < j.size();
        }
        set<size_t> s;
        bool hard_first;
    };
    
    comp compFunc = comp(cstr, hard_first);
    
    // split list
    forward_list<fptr>::iterator it = flist.begin();
    if(it != flist.end()) for(int i=0; i<maxLen; ++i) {
        ++it;
        if(it == flist.end()) break;
    }
    forward_list<fptr> sflist;
    sflist.splice_after(sflist.before_begin(), flist, flist.before_begin(), it);
    
    // only sort a portion of flist
    sflist.sort(compFunc);
    
    // merge list
    flist.splice_after(flist.before_begin(), sflist, sflist.before_begin(), sflist.end());
    
    /*debug message
    int cnt = 0;
    for(auto xd: flist) {
        cout << "f" << cnt << ":\n";
        for(auto a: xd->detAbility) cout << a << " ";
        cout << "\n========================\n";
        if(++cnt > 100) break;
    }
    */
}

set<size_t> ATPG::pat2cstr(const string& pat)
{
    set<size_t> ret;
    assert(pat.size() == cktin.size()+1);
    for(int i=0, n=pat.size(); i<n; ++i) {
        int c = ctoi(pat[i]);
        if(c != 2) {
            if(i == n-2) ret.insert(ptrNotCond(cktin[i]->inode[0], c==0));
            else if(i == n-1) ret.insert(ptrTimeShift(ptrNotCond(cktin[0]->inode[0], c==0)));
            else {
                ret.insert(ptrNotCond(cktin[i]->inode[0], c==0));
                ret.insert(ptrTimeShift(ptrNotCond(cktin[i+1]->inode[0], c==0)));
            }
        }
    }
    return ret;
}

vector<string> ATPG::randFillPat(const string& pat, const int& len)
{
    vector<string> ret;
    string rp(pat.size(), '0');
    for(int j=0; j<len; ++j) {
        for(int i=0, n=pat.size(); i<n; ++i) {
            if(pat[i]=='2' || pat[i]=='U') rp[i] = rand()&1 ? '0':'1';
            else rp[i] = pat[i];
        }
        ret.push_back(rp);
    }
    return ret;
}