#include <iostream>
#include <time.h>
#include <deque>
#include <stdlib.h>

#include "global.h"
#include "cpp_framework.h"
#include "configuration.h"
#include "parser.h"
#include "lru_stl.h"
#include "lru_pure.h"
#include "lru_bloomf.h"
#include <bf.h>

#include "stats.h"


using namespace std;

////////////////////////////////////////////////////////////////////////////////
//GLOBALS
////////////////////////////////////////////////////////////////////////////////

int totalSeqEvictedDirtyPages;

int totalNonSeqEvictedDirtyPages;

int totalEvictedCleanPages;

int threshold;

Configuration	_gConfiguration;
bool _gTraceBased = false;
TestCache<uint64_t, cacheAtom> * *_gTestCache; // pointer to each cache class in the hierachy
StatsDS *_gStats;
deque<reqAtom> memTrace; // in memory trace file

class hotness_table{
public:
    hotness_table(double in_fp, size_t in_capacity, int in_size)
    {
      size=in_size;
      capacity=in_capacity;
      fp=in_fp;
      for(int i=0;i<size;i++)
        bfilters.push_back(new bf::basic_bloom_filter(fp, capacity));
      m=bf::basic_bloom_filter::m(fp,capacity);
      k=bf::basic_bloom_filter::k(fp,capacity);
     
    }
    ~hotness_table()
    {
        for(list<bf::basic_bloom_filter*>::iterator it=bfilters.begin();it!=bfilters.end();++it)
            delete *it;
    }
    list<bf::basic_bloom_filter*> bfilters;
    
    
    void access(uint64_t key)
    {
        for(list<bf::basic_bloom_filter*>::iterator it=bfilters.begin();it!=bfilters.end();++it)
        {
            if((*it)->lookup(key)==0)
            {
                (*it)->add(key);
                break;
            }
            if(next(it)==bfilters.end())
            {
                
            }
                //in this case, all bms are filled, no need to record the access any more
        }
        
    }
    
    size_t get_hotness(uint64_t key){
        size_t hotness=0;
        for(list<bf::basic_bloom_filter*>::iterator it=bfilters.begin();it!=bfilters.end();++it)
        {
            if((*it)->lookup(key)==1)
            {
                hotness++;
            }
            
        }
        return hotness;
    
    }
    
    double fp;
    size_t capacity;
    int size;
    size_t m;
    size_t k;
    
    
};

int all_evicted_page_empty()
{
    int flag=1;
    for(int i=0;i<_gConfiguration.totalLevels;i++)
    {
        if(_gTestCache[i]->evict_empty())
            flag=1;
        else
        {
            flag=0;
            return 0;
        }
        return flag;
    }
}



void	readTrace(deque<reqAtom> & memTrace)
{
	assert(_gTraceBased); // read from stdin is not implemented
	_gConfiguration.traceStream.open(_gConfiguration.traceName, ifstream::in);

	if(! _gConfiguration.traceStream.good()) {
		PRINT(cout << " Error: Can not open trace file : " << _gConfiguration.traceName << endl;);
		ExitNow(1);
	}

	reqAtom newAtom;
	uint32_t lineNo = 0;

	while(getAndParseTrace(_gConfiguration.traceStream , &newAtom)) {
		///cout<<"lineNo: "<<newAtom.lineNo<<" flags: "<<newAtom.flags<<" fsblkn: "<<newAtom.fsblkno<<" issueTime: "<<newAtom.issueTime<<" reqSize: "<<newAtom.reqSize<<endl;

		///ziqi: if writeOnly is 1, only insert write cache miss page to cache
		if(_gConfiguration.writeOnly) {
			if(newAtom.flags & WRITE) {
#ifdef REQSIZE
				uint32_t reqSize = newAtom.reqSize;
				newAtom.reqSize = 1;

				//expand large request
				for(uint32_t i = 0 ; i < reqSize ; ++ i) {
					memTrace.push_back(newAtom);
					++ newAtom.fsblkno;
				}

#else
				memTrace.push_back(newAtom);
#endif
			}
		}
		///ziqi: if writeOnly is 0, insert both read & write cache miss page to cache
		else {
#ifdef REQSIZE
			uint32_t reqSize = newAtom.reqSize;
			newAtom.reqSize = 1;

			//expand large request
			for(uint32_t i = 0 ; i < reqSize ; ++ i) {
				memTrace.push_back(newAtom);
				++ newAtom.fsblkno;
			}

#else
			memTrace.push_back(newAtom);
#endif
		}

		assert(lineNo < newAtom.lineNo);
		IFDEBUG(lineNo = newAtom.lineNo;);
		newAtom.clear();
	}

	_gConfiguration.traceStream.close();
}

void	Initialize(int argc, char **argv, deque<reqAtom> & memTrace)
{
	if(!_gConfiguration.read(argc, argv)) {
		cerr << "USAGE: <TraceFilename> <CfgFileName> <TestName>" << endl;
		exit(-1);
	}

	readTrace(memTrace);
	assert(memTrace.size() != 0);
	//Allocate StatDs
	_gStats = new StatsDS[_gConfiguration.totalLevels];
	//Allocate hierarchy
	_gTestCache = new TestCache<uint64_t, cacheAtom>*[_gConfiguration.totalLevels];

	for(int i = 0; i < _gConfiguration.totalLevels ; i++) {
		if(_gConfiguration.GetAlgName(i).compare("pagelru") == 0) {
			_gTestCache[i] = new PageLRUCache<uint64_t, cacheAtom>(cacheAll, _gConfiguration.cacheSize[i], i);
		}
    
		else if(_gConfiguration.GetAlgName(i).compare("purelru") == 0) {
			_gTestCache[i] = new PureLRUCache<uint64_t, cacheAtom>(cacheAll, _gConfiguration.cacheSize[i], i);
		}
		else if(_gConfiguration.GetAlgName(i).compare("bflru") == 0) {
			_gTestCache[i] = new BFLRUCache<uint64_t, cacheAtom>(cacheAll, _gConfiguration.cacheSize[i], i);
		}
//esle if //add new policy name and dynamic allocation here
		else {
			cerr << "Error: UnKnown Algorithm name " << endl;
			exit(1);
		}
	}

	PRINTV(logfile << "Configuration and setup done" << endl;);
	srand(0);
}
void reportProgress()
{
	static uint64_t totalTraceLines = memTrace.size();
	static int lock = -1;
	int completePercent = ((totalTraceLines - memTrace.size()) * 100) / totalTraceLines ;

	if(completePercent % 10 == 0 && lock != completePercent) {
		lock = completePercent ;
		std::cerr << "\r--> " << completePercent << "% done" << flush;
	}

	if(completePercent == 100)
		std::cerr << endl;
}

/*backup
void recordOutTrace( int level, reqAtom newReq){
	if(_gConfiguration.outTraceStream[level].is_open()){
		_gConfiguration.outTraceStream[level] << newReq.issueTime << "," <<"OutLevel"<<level<<",0,";

		//_gConfiguration.outTraceStream[level] <<"flags: "<< newReq.flags <<" !";

		if(newReq.flags & READ){
			_gConfiguration.outTraceStream[level] << "Read,";
		}
		else
			_gConfiguration.outTraceStream[level] << "Write,";
		//FIXME: check math
		_gConfiguration.outTraceStream[level] << newReq.fsblkno * 512 * 8 <<","<< newReq.reqSize * 512 << endl;

	}
}
*/

///ziqi: this has no use by Jun 19 2013
void recordOutTrace(int level, reqAtom newReq)
{
	/*
	  if(_gConfiguration.outTraceStream[level].is_open()) {
	      _gConfiguration.outTraceStream[level] << newReq.issueTime << "!";
	      //_gConfiguration.outTraceStream[level] <<"flags: "<< newReq.flags <<" !";

	      if(newReq.flags & READ){
	      	_gConfiguration.outTraceStream[level] << "Read,";
	      }
	      else
	      	_gConfiguration.outTraceStream[level] << "Write,";

	      //FIXME: check math
	      _gConfiguration.outTraceStream[level] << newReq.fsblkno << endl;
	      //_gConfiguration.outTraceStream[level] << newReq.flags << endl;
	  }
	*/
}

void runDiskSim()
{
	std::string command = _gConfiguration.diskSimPath;
	command += _gConfiguration.diskSimuExe;
	command += " ";
	command += _gConfiguration.diskSimPath;
	command += _gConfiguration.diskSimParv;
	command += " ";
	command += _gConfiguration.diskSimPath;
	command += _gConfiguration.diskSimOutv;
	command += " ascii ";
	//command += _gConfiguration.cache2diskPipeFileName;
	///ziqi: the line above is by Alireza. I use diskSimInputTraceName to denote the DiskSim input trace file name
	command += _gConfiguration.diskSimInputTraceName;
	command += " 0";
	PRINTV(logfile << "Running Disk Simulator with following command:" << endl;);
	PRINTV(logfile << command << endl;);
	system(command.c_str());
}

void runSeqLengthAnalysis()
{
	std::string command = _gConfiguration.analysisAppPath;
	command += _gConfiguration.analysisAppExe;
	command += " ";
	command += _gConfiguration.diskSimInputTraceName;
	command += " ";
	command += "analyzed-" + _gConfiguration.diskSimInputTraceName;
	PRINTV(logfile << "Running Seq Length Analysis App with following command:" << endl;);
	PRINTV(logfile << command << endl;);
	system(command.c_str());
}

void RunBenchmark(deque<reqAtom> & memTrace)
{
	PRINTV(logfile << "Start benchmarking" << endl;);
    hotness_table hot_table(0.8,100000,20);
    
	//main simulation loop
	while(! memTrace.empty()) {
		uint32_t newFlags = 0;
		reqAtom newReq = memTrace.front();
		cacheAtom newCacheAtom(newReq);
        //wei xie: record which level it hits
        uint32_t hitLayer=_gConfiguration.totalLevels;
		//access hierachy from top layer, reqSize is always equal to 1
		for(int i = 0 ; i < _gConfiguration.totalLevels ; i++) {
			//access cache at level i for newReq
			//BUG: victim dirty pages from upper levels does not access lower levels
			newFlags = _gTestCache[i]->access(newReq.fsblkno, newCacheAtom, newReq.flags);
            
            
            //TODO: need to determine if evicted page needs to be demoted to lower level or discarded
			collectStat(i, newFlags);
			
			//BUG: write requests in the upper-level write-back cache does not access lower levels even
			/* in the case of cache miss
			 */
			
			if(newFlags & PAGEHIT)
            {
                hitLayer=i;
				break; // no need to check further down in the hierachy
            }
			recordOutTrace(i, newReq);
			newFlags = 0; // reset flag
		}
		
        //load cache to the correct layer
        int hotness=hot_table.get_hotness(newReq.fsblkno);
        cout<<"Hotness of key "<<newReq.fsblkno<<" is "<<hotness<<endl;
        getchar();
        hot_table.access(newReq.fsblkno);
        /*
        int newLayer=get_layer_promote(hotness,hitLayer);
        
		if(newLayer!=hitLayer)
        {
            //remove from hit layer
            if(hitLayer!=_gConfiguration.totalLevels)
                _gTestCache[hitLayer]->remove(newReq.fsblkno,newCacheAtom);
            //insert into target layer
                _gTestCache[newLayer]->insert(newReq.fsblkno,newCacheAtom);
        }
        */
        //handle evicted pages
        //need to know what pages are evicted
        //there are two cases a page is evicted
        //(1) on hit at layer i, and it decides to promote the page to layer j (j<i), if this layer is full, a page is evicted
        //(2) if miss at all layers, it devides to load the page to layer j, if this layer is full, a page is evicted
        //it is noted that an evicted may be demoted to a lower level cache, which will result a chain of evictions
        
        //if there are still evicted pages, continue
        //TODO implement all_evicted_page_empty()
        /*
        while(!all_evicted_page_empty())
        {
            //handle evicted pages 1 layer a time
            for(int i = 0 ; i < _gConfiguration.totalLevels ; i++) {
                list<uint64_t> evict_list(_gTestCache[i]->get_evict_entries());
                
                //iterate through all evicted pages
                while(!evict_list.empty()){
                    uint64_t key=evict_list.back();
                    evict_list.pop_back();
                    int hotness=hot_table.get_hotness(key);
                    int newLayer=get_layer_demote(hotness,i);
                    if(newLayer!=_gConfiguration.totalLevels)
                        //insert into target layer
                        _gTestCache[newLayer]->insert(key,newCacheAtom);
                }
                
                //lookup bloom filter to see the hotness
                
                //demote to a lower layer or write-back (dirty) or discard (clean)

            }
        }
        */

		memTrace.pop_front();
		reportProgress();
	}

	if(! _gConfiguration.diskSimuExe.empty()) {
		PRINTV(logfile << "Multi-level Cache Simulation is Done, Start Timing Simulation with Disk simulator" << endl;);
		runDiskSim();
	}

	if(! _gConfiguration.analysisAppExe.empty()) {
		PRINTV(logfile << "Timing Simulation is Done, Start Sequential Length Analysis" << endl;);
		runSeqLengthAnalysis();
	}

	PRINTV(logfile << "Benchmarking Done" << endl;);
}

int main(int argc, char **argv)
{
	totalEvictedCleanPages = 0;
	totalSeqEvictedDirtyPages = 0;
	totalNonSeqEvictedDirtyPages = 0;
	//read benchmark configuration
	Initialize(argc, argv, memTrace);

	if(_gConfiguration.GetAlgName(0).compare("dynamiclru") == 0
			|| _gConfiguration.GetAlgName(0).compare("dynamicBlru") == 0
			|| _gConfiguration.GetAlgName(0).compare("dynamicClru") == 0
			|| _gConfiguration.GetAlgName(0).compare("hotcoldlru") == 0
			|| _gConfiguration.GetAlgName(0).compare("purelru") == 0
			|| _gConfiguration.GetAlgName(0).compare("arc") == 0
			|| _gConfiguration.GetAlgName(0).compare("darc") == 0
			|| _gConfiguration.GetAlgName(0).compare("larc") == 0
			|| _gConfiguration.GetAlgName(0).compare("ldarc") == 0) {
		threshold = 1;
	}
	else
		threshold = _gConfiguration.seqThreshold;

	RunBenchmark(memTrace); // send reference memTrace
	ExitNow(0);
}
