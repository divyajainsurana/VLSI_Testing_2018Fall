#include "atpg.h"
#include <cassert>

static string getFileDir(const string& filePath)
{
    size_t pos = filePath.find_last_of("/");
    if(pos == string::npos) return string("./");
    return filePath.substr(0, pos+1);
}

ATPG::FaultDict::FaultDict(const size_t& fSize, const size_t& pSize)
{
    fpTbl = vector<vector<bool> >(fSize, vector<bool>(pSize, 0));
}

void ATPG::FaultDict::toggle(const size_t& fid, const size_t& pid)
{
    fpTbl[fid][pid] = 1;
}

void ATPG::FaultDict::toILP(const string& fileName, const int& ndet)
{
    vector<string> varName;
    varName.reserve(fpTbl[0].size());
    for(size_t i=0, n=fpTbl[0].size(); i<n; ++i)
        varName.push_back(string("v")+to_string(i));
    
    ofstream fp;
    fp.open(fileName.c_str());
    
    // write objective
    fp << "min: ";
    for(size_t i=0, n=varName.size(); i<n; ++i) {
        fp << varName[i];
        if(i != n-1) fp << " + ";
        else fp << ";\n";
    }
    
    // write constraints
    string ndet_s = to_string(ndet);
    for(auto bv: fpTbl) {
#if !defined( NDEBUG )
        assert(count(bv.begin(), bv.end(), 1) >= ndet);
#endif
        bool first = true;
        for(size_t i=0, n=bv.size(); i<n; ++i) if(bv[i]) {
            if(first) first = false;
            else fp << " + ";
            fp << varName[i];
        }
        fp << " >= " << ndet_s << ";\n";
    }
    for(string vn: varName) fp << "1 >= " << vn << " >= 0;\n";
    
    // write var declaration
    fp << "int ";
    for(size_t i=0, n=varName.size(); i<n; ++i) {
        fp << varName[i];
        if(i != n-1) fp << ", ";
        else fp << ";\n";
    }
    
    fp.close();
}

ATPG::FaultDict* ATPG::buildFaultDict(const bool& simFirst)
{
    if(simFirst) {
        flist_undetect.clear();
        for(auto it=flist.begin(); it!=flist.end(); ++it) {
            fptr f = (*it).get();
            f->detected_time = 0;
            f->detect = FALSE;
            f->detAbility.clear();
            flist_undetect.push_front(f);
        }
        int dummy1, dummy2;
        for(string pat: vectors)
            tdfault_sim_a_vector(pat, dummy1, dummy2);
    }
    
    size_t fSize = 0;
    map<fptr, size_t> fptr2idx;
    
    flist_undetect.clear();
    for(auto it=flist.begin(); it!=flist.end(); ++it) {
        fptr f = (*it).get();
        if(f->detect) fptr2idx[f] = fSize++;
        f->detected_time = 0;
        f->detect = FALSE;
        flist_undetect.push_front(f);
    }
    
    if(!fSize || vectors.empty()) {
        cerr << "no fault detected / empty pattern vectors !!\n";
        return NULL;
    }
    
    // fault dict size constraint: max fSize*pSize
    const size_t maxSize = 40500000;
    if(fSize * vectors.size() > maxSize) {
        cerr << "problem size (" << fSize << "*" << vectors.size() << ") exceeds limit!!\n";
        return NULL;
    }
    
    FaultDict* fd = new(nothrow) FaultDict(fSize, vectors.size());
    if(!fd) {
        cerr << "not enough room for complete fault dictionary...\n";
        return NULL;
    }
    
    string pat;
    int dummy1, dummy2;
    for(size_t i=0, n=vectors.size(); i<n; ++i) {
        pat = vectors[i];
        tdfault_sim_a_vector(pat, dummy1, dummy2, false);
        for(fptr f: flist_undetect) if(f->detected_time) {
            fd->toggle(fptr2idx[f], i);
            f->detected_time = 0;
            f->detect = FALSE;
        }
    }
#if !defined( NDEBUG )    
    cout << "fault dict size: " << fSize << " * " << vectors.size() << endl;
#endif
    return fd;
}

vector<size_t> ATPG::FaultDict::getILPSol(const string& fileName)
{
    vector<size_t> ret;
    ifstream fp(fileName.c_str());
    
    // exception: file not opened
    if(!fp.is_open()) {
        ret.push_back(fpTbl[0].size());
        return ret;
    }
    
    string line;
    bool start = false;
    size_t cnt = 0;
    while(getline(fp, line)) {
        if(start) {
            if(line.back() == '1') ret.push_back(cnt);
            cnt++;
        }
        if(line == string("Actual values of the variables:")) start = true;
    }
    fp.close();
    
    // exception: no index read
    if(ret.empty()) ret.push_back(fpTbl[0].size());
    
    return ret;
}

void ATPG::solveQMILP(const bool& simFirst, const size_t& minVecSize)
{
    size_t origVecSize = vectors.size();
    
    // exception: c6288.ckt
    bool isC6288 = (cktin.size()==32) && (cktout.size()==32) && (sort_wlist.size()==4832);
    
    // pad the vectors to minVecSize
    bool pad = origVecSize < minVecSize;
    if(pad && !isC6288) randGenPats(minVecSize);
    
    FaultDict* fd = buildFaultDict(simFirst);
    if(!fd) return;
    
    string ilpIn("qm.ilp"), ilpOut("qm.sol"), ilpSolv("lp_solve");
    fd->toILP(ilpIn, detected_num);
    
    // system call ILP solver
    string command("./");
    command += getFileDir(execFilePath) + ilpSolv;;
    command += string(" -lp ") + ilpIn;
    
    // set timeout constraint
    double tCurr = (double)clock() / CLOCKS_PER_SEC;
    double tMax = 570.0;  // ILP has to be done before 9m30s
    double tRemain = tMax - tCurr;
    command += string(" -timeout ") + to_string(tRemain);
    
    // redirect
    command += string(" > ") + ilpOut;
    
    int stat = system(command.c_str());  // timeout with suboptimal: 256
    if(!stat || (stat==256)) {
        vector<size_t> keep = fd->getILPSol(ilpOut);
        if(keep[0] != vectors.size()) {
            vector<string> tmp;
            for(size_t i: keep) tmp.push_back(vectors[i]);
#if !defined( NDEBUG )
            cout << "before QM: " << vectors.size() << ", after: " << tmp.size() << "\n";
#endif
            vectors = tmp;
        }
    }
    
    // free fault dict
    delete fd;
    
    // QM fail (timeout or whatever reason)
    if(vectors.size() > origVecSize) vectors.resize(origVecSize);
    if(stat) simpleSTC();
}

void ATPG::simpleSTC()
{
    // reset flist_undetect
    flist_undetect.clear();
    for(auto it=flist.begin(); it!=flist.end(); ++it) {
        fptr f = (*it).get();
        f->detected_time = 0;
        f->detect = FALSE;
        f->detAbility.clear();
        flist_undetect.push_front(f);
    }
    
    // re-order patterns
    //reverse(vectors.begin(), vectors.end());          // reverse order
    random_shuffle(vectors.begin(), vectors.end());   // random order
    
    // tdfsim
    int useful, current_detect_num=0;
    vector<string> newPats;
    for(string pat: vectors) {
        useful = FALSE;
        tdfault_sim_a_vector(pat, current_detect_num, useful);
        if(useful == TRUE) newPats.push_back(pat);
    }
    
#if !defined( NDEBUG )
    cout << "before simple STC: " << vectors.size() << ", after: " << newPats.size() << "\n";
#endif
    
    // update patterns
    vectors = newPats;
}

void ATPG::randGenPats(const size_t& toSize)
{
    if(vectors.size() >= toSize) return;
    vector<string> randVec = randFillPat(string(cktin.size()+1, '2'), toSize-vectors.size());
    vectors.insert(vectors.end(), randVec.begin(), randVec.end());
}
