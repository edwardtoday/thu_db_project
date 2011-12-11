#include "FZ_indexer.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <string>
#include <utility>
#include <algorithm>
#include <ctime>
using namespace std;
const int MAX_FILENAME_LENGTH = 64;
struct TestFile
{
	char dataFile[MAX_FILENAME_LENGTH];
	char queryFile[MAX_FILENAME_LENGTH];
	char resultFile[MAX_FILENAME_LENGTH];
};

template<class T>
bool AssertVectorEqual(const vector<T>& results, const vector<T>& correctResults)
{
	if (results.size() != correctResults.size())
		return false;
	for (unsigned i=0; i<results.size(); ++i)
	{
		if (results[i] != correctResults[i])
			return false;
	}
	return true;
}
void LoadQueryInfo(const char* queryFile, vector<pair<string,unsigned> >& queryInfos)
{
	ifstream in(queryFile);
	if (in.fail())
	{
		printf("Can not open %s\n", queryFile);
		exit(0);
	}
	string line;
	while (getline(in, line, '\n') != NULL)
	{
		string::size_type pos = line.rfind('\t');
		if (pos == string::npos)
			continue;
		string query = line.substr(0,pos);
		unsigned edThreshold = atoi(line.substr(pos+1).c_str());
		queryInfos.push_back( pair<string,unsigned>(query,edThreshold) );
	}
	in.close();
}
void LoadCorrectResults(const char* resultFile, vector<vector<unsigned>  >& correctResults)
{
	ifstream in(resultFile);
	if (in.fail())
	{
		printf("Can not open %s\n", resultFile);
		exit(0);
	}
	string line;
	while (getline(in, line, '\n') != NULL)
	{
		string::size_type pos = line.rfind('\t');
		if (pos == string::npos)
			continue;
		string query = line.substr(0,pos);
		unsigned similarWordsNum = atoi(line.substr(pos+1).c_str());

		vector<unsigned> correctResult;
		for (unsigned i=0; i<similarWordsNum; ++i)
		{
			getline(in,line,'\n');
			string::size_type pos = line.find('\t');
			assert(pos != string::npos);
			unsigned similarWordId = atoi(line.substr(0,pos).c_str());
			correctResult.push_back(similarWordId);
		}
		correctResults.push_back(correctResult);
	}
	in.close();
}
bool ValidateCorrectness(const FZ_Indexer& fz_indexer, const TestFile& testFile)
{
	vector<pair<string,unsigned> > queryInfos;  // Pair first: query; Pair second: edThreshold
	LoadQueryInfo(testFile.queryFile, queryInfos);

	vector<vector<unsigned>  > correctResults;
	LoadCorrectResults(testFile.resultFile, correctResults);

	unsigned totalTestCase = queryInfos.size();
	unsigned passCaseNum = 0;
	for (unsigned i=0; i<queryInfos.size(); ++i)
	{
		vector<unsigned> results;
		fz_indexer.Search(queryInfos[i].first.c_str(), queryInfos[i].second, results);
		if (AssertVectorEqual<unsigned>(results,correctResults[i]))
		{
			++passCaseNum;
		}
	}
	cerr << "* DataFile = " << testFile.dataFile << "\t"
		   <<  "TotalCase = " << totalTestCase << "\t"
		   << "PassCase = " << passCaseNum << endl;
	//printf("DataFile : %s    TotalCase : %u    PassCase : %u\n", testFile.dataFile, totalTestCase, passCaseNum);
	return totalTestCase == passCaseNum;
}
void ComputeEfficiency(const FZ_Indexer& fz_indexer, const TestFile& testFile)
{
	vector<pair<string,unsigned> > queryInfos;  // Pair first: query; Pair second: edThreshold
	LoadQueryInfo(testFile.queryFile, queryInfos);

	clock_t minTime=unsigned(1<<31)-1, maxTime=0,totalTimeSum=0;
	for (unsigned i=0; i<queryInfos.size(); ++i)
	{
		vector<unsigned> results;
		clock_t begin = clock();
		fz_indexer.Search(queryInfos[i].first.c_str(), queryInfos[i].second, results);
		clock_t end = clock();
		totalTimeSum += end-begin;
		if (minTime>end-begin) minTime = end-begin;
		if (maxTime<end-begin) maxTime = end-begin;
	}
	cerr << "* DataFile = " << testFile.dataFile << "\t" 
		<< "MinTime = " <<  minTime*1000.0/CLOCKS_PER_SEC << "\t" 
		<< "MaxTime = " << maxTime*1000.0/CLOCKS_PER_SEC << "\t"
		<< "AvgTimeSum = " << totalTimeSum*1000.0/(queryInfos.size()*CLOCKS_PER_SEC) << "\t" << endl;
	//printf("DataFile : %s    MinTime : %.0lfms    MaxTime : %.3lfms    AvgTime : %.3lfms\n", 
	//	testFile.dataFile, minTime*1000.0/CLOCKS_PER_SEC, maxTime*1000.0/CLOCKS_PER_SEC, totalTimeSum*1000.0/(queryInfos.size()*CLOCKS_PER_SEC));
}
bool TestFZ_Indexer(const FZ_Indexer& fz_indexer, const TestFile& testFile)
{
	if (!ValidateCorrectness(fz_indexer,testFile))
	{
			return false;
	}
  else
  {
  	ComputeEfficiency(fz_indexer,testFile); 
  }
	return true;
}
int main()
{
//	TestFile testFiles[]  ={ {"dblp_data.txt","dblp_data.txt_query_1","dblp_data.txt_result_1"}};
	TestFile testFiles[]  ={ {"dblp_sample_data.txt","dblp_sample_query.txt","dblp_sample_result.txt"},
	{"dblp_data.txt","dblp_data.txt_query_1","dblp_data.txt_result_1"},
	{"url_data.txt","url_data.txt_query_1","url_data.txt_result_1"},
	{"dblp_data.txt","dblp_data.txt_query_2","dblp_data.txt_result_2"},
	{"url_data.txt","url_data.txt_query_2","url_data.txt_result_2"}
	};

	unsigned testFileNumber = sizeof(testFiles)/sizeof(TestFile);
	int nright = 0;
	for (unsigned i=0; i<testFileNumber; ++i)
	{
		FZ_Indexer fz_indexer(testFiles[i].dataFile);
		clock_t indexBegin = clock();
		fz_indexer.CreateIndex();
		fz_indexer.LoadIndex();
		clock_t indexEnd = clock();
		cerr << "* DataFile = " << testFiles[i].dataFile << "\t"
		     << "IndexTime = " << (indexEnd-indexBegin)*1.0/(CLOCKS_PER_SEC) << endl;
		if (!TestFZ_Indexer(fz_indexer, testFiles[i]))
			break;
		++nright;
		fz_indexer.DestroyIndex();
	}
	cerr << "* RightDataSet = " << nright << endl;
	return 0;
}
