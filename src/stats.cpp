#include <sys/types.h>
#include <sys/stat.h>
#include "stats.h"
#include "global.h"

using namespace std; 
extern StatsDS _gStats; 

void collectStat( uint32_t newFlags){
	
	++ _gStats.Ref;
	
	// find read or write count
	
	if(newFlags	&	READ){
		++_gStats.PageRead ;
		// Collect Read stats
		if(newFlags	&	PAGEHIT){
			++ _gStats.PageReadHit;
			assert( newFlags & BLKHIT);
			assert( !(newFlags & PAGEMISS));
		}
		if(newFlags	&	PAGEMISS)
			++ _gStats.PageReadMiss;
		if(newFlags	&	BLKHIT){
			++ _gStats.BlockWriteHit;
			assert( !(newFlags & BLKMISS) );
		}
		if(newFlags	&	BLKMISS){
			++ _gStats.BlockReadMiss;
			++ _gStats.PageReadMiss;
		}
	}
	else if(newFlags	&	WRITE){
		++_gStats.PageWrite;
		// Collect Read stats
		if(newFlags	&	PAGEHIT){
			++ _gStats.PageWriteHit;
			assert( newFlags & BLKHIT);
			assert( !(newFlags & PAGEMISS));
		}
		
		if(newFlags	&	BLKHIT){
			++ _gStats.BlockWriteHit;
			assert( !(newFlags & BLKMISS) );
			if(newFlags	&	PAGEMISS)
				++ _gStats.PageWriteMiss;
		}
		if(newFlags	&	BLKMISS){
			assert( !(newFlags & BLKHIT) );
			++ _gStats.BlockWriteMiss;
			++ _gStats.PageWriteMiss;
		}
		if(newFlags	&	EVICT)
			++ _gStats.BlockEvict;
		
		if(newFlags	&	PAGEMISS && ! (newFlags	&BLKHIT ) && !(newFlags	&BLKMISS) ) // for page based algorithm
			++ _gStats.PageWriteMiss;
		
		if( newFlags & COLD2COLD ){
			++ _gStats.Cold2Cold;
			assert( ! (newFlags & COLD2HOT) ); 
		}
		if(newFlags & COLD2HOT)
			++ _gStats.Cold2Hot; 
			
	}
	else{
		cerr<<"Error: Unknown request type in stat collection"<<endl;
		assert(0);
	}
}

// print histograms
void printHist(){
	ofstream pirdStream,birdStream;
	string pirdName("Stats/");
	pirdName.append(_gConfiguration.testName);
	pirdName.append("-");
	pirdName.append(_gConfiguration.GetAlgName());
	string birdName(pirdName);
	pirdName.append(".PIRD");
	birdName.append(".BIRD");
	
	pirdStream.open(pirdName, ios::out|ios::trunc);
	if( ! pirdStream.good() ){
		cerr<<"Error: can not open PIRD file: "<<pirdName<<endl;
		return;
	}
	birdStream.open(birdName, ios::out|ios::trunc);
	if( ! birdStream.good() ){
		cerr<<"Error: can not open BIRD file: "<<birdName<<endl;
		return;
	}
	
	for(unsigned i = 0; i < _gConfiguration.futureWindowSize  ; ++i ){
		pirdStream<<i<<"\t"<<_gConfiguration.pirdHist[i]<<endl;
		birdStream<<i<<"\t"<<_gConfiguration.birdHist[i]<<endl;
	}
	pirdStream.close();
	birdStream.close();
}

//print stats
void printStats(){
	
	ofstream statStream;
	mkdir("Stats", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	string fileName("Stats/");
	fileName.append(_gConfiguration.testName);
	fileName.append(".stat");
	statStream.open(fileName, ios::out|ios::app);
	if( ! statStream.good() ){
		cerr<<"Error: can not open stat file: "<<fileName<<endl;
		return;
	}
	
	statStream<<_gConfiguration.testName<<",\t"<<_gConfiguration.GetAlgName()<<endl;
	Stat * tempStat;
	while( ( tempStat = _gStats.next() ) ){
		statStream<< tempStat->print() <<endl;
	}
	statStream<<endl;
	statStream.close();

	IFHIST(printHist(););
	
}