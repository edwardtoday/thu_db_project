// a dummy header file
#pragma once
#include <map>
#include <vector>
#include <string>

using namespace std;

#define ID_List vector<unsigned int>
#define GramIndex map<string, ID_List*>
#define Dictionary map<unsigned int, string>
#define LenIndex map<unsigned int, GramIndex*>

#define QGRAM 2
#define LINE_LENGTH 256

typedef int RC; // type for returned codes

const int SUCCESS = 1;
const int FAILURE = 0;

class FZ_Indexer {
private:
	string originDataFileName;
public:
	mutable LenIndex index;
	mutable Dictionary words;
public:
	FZ_Indexer(const char *dataFileName);
	~FZ_Indexer();

	RC
	CreateIndex(); // create an index and store it in a file

	RC
	DestroyIndex(); // destroy the index file on disk

	RC
	LoadIndex(); // Load the index from disk if it's not in memory

	RC
	Search(const char *query, const unsigned edThreshold,
			vector<unsigned> &results) const;
private:

	RC
	SaveIndex();

	RC
	line2gram(char * line, unsigned str_num, GramIndex * child);

	void
	Filter(const char *query, vector<ID_List *> &vectors,
			const unsigned edThreshold) const;

	RC
	Refine(const char *query, unsigned num, unsigned edThreshold) const;

	static void
	ScanCount(vector<ID_List *> lists, int T, vector<unsigned> &result,
			const unsigned size);
};
