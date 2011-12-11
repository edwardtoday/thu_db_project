#include "FZ_indexer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <pthread.h>
#include <memory.h>
//#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>

//if you want to disable multi-thread computing, comment the following line
//#define THREADS 4

using namespace std;

typedef pair<string, ID_List*> p1;
typedef pair<unsigned int, GramIndex*> p2;
typedef pair<unsigned int, string> p3;

//	used for split string
void Tokenize(const string& str, vector<string>& tokens,
		const string& delimiters = " ") {
	// Skip delimiters at beginning.
	string::size_type lastPos = str.find_first_not_of(delimiters, 0);
	// Find first “non-delimiter”.
	string::size_type pos = str.find_first_of(delimiters, lastPos);

	while (string::npos != pos || string::npos != lastPos) {
		// Found a token, add it to the vector.
		tokens.push_back(str.substr(lastPos, pos - lastPos));
		// Skip delimiters. Note the “not_of”
		lastPos = str.find_first_not_of(delimiters, pos);
		// Find next “non-delimiter”
		pos = str.find_first_of(delimiters, lastPos);
	}
}

//get the dataFileName for current instance
FZ_Indexer::FZ_Indexer(const char *dataFileName) {
	//	originDataFileName = (char*) malloc(strlen(dataFileName) + 100);
	//	memcpy(originDataFileName, dataFileName, strlen(dataFileName));
	string fn(dataFileName, strlen(dataFileName));
	originDataFileName = fn;
}

//release mem for index and words
FZ_Indexer::~FZ_Indexer() {
	if (index.size() > 0) {
		for (LenIndex::iterator it = index.begin(); it != index.end(); it++)
			delete it->second;
		index.clear();
	}
	if (words.size() > 0)
		words.clear();
	//	delete originDataFileName;
}

/*
 * input: query, edit-distance threshold
 * output: matching wordIDs in a vector
 */
RC FZ_Indexer::Search(const char *query, const unsigned edThreshold, vector<
		unsigned> &results) const {
	//Search using the Fliter->ScanCount->Refine method
	vector<ID_List *> vectors;
	vector<unsigned> merge_result;
	Filter(query, vectors, edThreshold);
	int t = strlen(query) + 1 - QGRAM - edThreshold * QGRAM;	//calc the threshold for ScanCount
	ScanCount(vectors, t, merge_result, words.size());
	for (unsigned i = 0; i < merge_result.size(); i++) {
		if (Refine(query, merge_result[i], edThreshold))
			results.push_back(merge_result[i]);
	}
	return SUCCESS;
}

//read the datafile to create a wordlist "word" and index tree "index"
RC FZ_Indexer::CreateIndex() {
	ifstream fin;
	fin.open(originDataFileName.c_str());
	if (!fin) {
		cerr << "Data File Open Error!\n";
		return FAILURE;
	}
	char line[LINE_LENGTH];
	unsigned str_num = 0;
	while (!fin.eof()) {
		fin.getline(line, LINE_LENGTH, '\n');

		if (strlen(line) != 0) {
			string te(line, strlen(line) - 1);
			words[str_num] = te;
			LenIndex::iterator it = index.find(strlen(line) - 1);
			if (it == index.end()) {
				GramIndex* value = new GramIndex();
				line2gram(line, str_num, value);
				index[strlen(line) - 1] = value;
			} else {
				line2gram(line, str_num, it->second);
			}
			str_num++;
		}
	}
	fin.close();
	//	cout << "words.size " <<words.size()<<endl;
	//	return SUCCESS;
	return SaveIndex();
}


//save current index in mem to fs
RC FZ_Indexer::SaveIndex() {
	string indexname(this->originDataFileName);
	indexname.append(".fzindex");
	ofstream fout;
	fout.open(indexname.c_str());
	if (!fout) {
		cerr << "Index File Open Error!\n";
		return FAILURE;
	}
	GramIndex * itchild = new GramIndex();
	for (LenIndex::iterator iter = index.begin(); iter != index.end(); iter++) {
		itchild = iter->second;
		fout << iter->first << "," << itchild->size() << endl;
		for (GramIndex::iterator it = itchild->begin(); it != itchild->end(); it++) {
			fout << it->first << it->second->size() << ",";
			for (unsigned i = 0; i < it->second->size(); i++)
				fout << it->second->at(i) << ",";
			fout << endl;
		}
	}
	fout.flush();
	fout.close();

	return SUCCESS;
}

RC FZ_Indexer::DestroyIndex() {
	string indexname(this->originDataFileName);
	indexname.append(".fzindex");

	if (remove(indexname.c_str()) == -1)
		return FAILURE;
	return SUCCESS;
}

//recover index from fs to mem
//word list is recovered directly from datafile
RC FZ_Indexer::LoadIndex() {
	// init
	//	cout << words.size() << "\t" << index.size() << endl;
	words.clear();
	// Load Dictionary
	ifstream fin;
	fin.open(originDataFileName.c_str());
	if (!fin) {
		cerr << "Data File Open Error!\n";
		return FAILURE;
	}
	char curline[LINE_LENGTH];
	unsigned str_num = 0;
	while (!fin.eof()) {
		//		while (curline[strlen(curline) - 1] == '\r') {
		//			curline[strlen(curline) - 1] = '\0';
		//		}
		fin.getline(curline, LINE_LENGTH, '\n');
		if (strlen(curline) != 0) {
			string te(curline, strlen(curline) - 1);
			words[str_num] = te;
			str_num++;
		}
	}
	fin.close();
	// Load index image
	string indexname(this->originDataFileName);
	indexname.append(".fzindex");
	index.clear();
	fin.open(indexname.c_str());
	if (!fin) {
		cerr << "Index File Open Error!\n";
		return FAILURE;
	}
	string str;
	unsigned firstkey = 0, valuesum = 0, linenum = 0, splitsum;
	char splitchar;
	while (!fin.eof()) {
		getline(fin, str);
		stringstream keyin(str);
		keyin >> firstkey >> splitchar >> valuesum;
		GramIndex * child = new GramIndex();
		for (unsigned i = 0; i < valuesum; i++) {
			char key[QGRAM + 1] = { 0 };
			fin.read(key, QGRAM);
			string map_key(key);
			getline(fin, str);
			stringstream isin(str);
			isin >> splitsum >> splitchar;
			ID_List * value = new ID_List();
			for (unsigned i = 0; i < splitsum; i++) {
				isin >> linenum >> splitchar;
				value->push_back(linenum);
			}
			if (map_key.length() != 0)
				child->insert(GramIndex::value_type(map_key, value));
		}
		index.insert(LenIndex::value_type(firstkey, child));
	}
	fin.close();

	//	cout << words.size() << "\t" << index.size() << endl;
	return SUCCESS;
}

//process a line in datafile to Q-grams; then add the to GramIndex "child"
RC FZ_Indexer::line2gram(char * line, unsigned str_num, GramIndex * child) {
	char key[QGRAM + 1];
	string map_key;
	unsigned length = strlen(line);
	for (unsigned loc = 0; loc < length - QGRAM; loc++) {
		memcpy(key, line + loc, QGRAM);
		key[QGRAM] = 0;
		map_key.assign(key);
		GramIndex::iterator iter = child->find(map_key);
		if (iter == child->end()) {
			ID_List* newvalue = new ID_List();
			newvalue->push_back(str_num);
			child->insert(GramIndex::value_type(map_key, newvalue));
			;
		} else {
			if (iter->second->at(iter->second->size() - 1) != str_num)
				iter->second->push_back(str_num);
		}
	}
	return SUCCESS;
}

//get the possible results
void FZ_Indexer::Filter(const char *query, vector<ID_List *> &vectors,
		const unsigned edThreshold) const {
	char key[QGRAM + 1];
	string map_key;
	unsigned length = strlen(query);
	unsigned upbound_i = length + edThreshold + 1;
	for (unsigned i = length - edThreshold; i < upbound_i; i++) {
		LenIndex::iterator it = index.find(i);
		if (it == index.end())
			continue;
		else {
			//			unsigned upbound_loc = length + 1 - QGRAM;
			for (unsigned loc = 0; loc < length + 1 - QGRAM; loc++) {
				memcpy(key, query + loc, QGRAM);
				key[QGRAM] = 0;
				map_key.assign(key);
				GramIndex::iterator iter = it->second->find(map_key);
				if (iter != it->second->end()) {
					vectors.push_back(iter->second);
				}
			}
		}
	}
}

//check if a possible result is valid
RC FZ_Indexer::Refine(const char *query, unsigned num, unsigned edThreshold) const {
	//get the string to compare
	char queryx[LINE_LENGTH];
	strncpy(queryx, query, LINE_LENGTH);
	string str = words[num];
	char tomatch[str.length() + 1];
	memset(tomatch, 0, strlen(tomatch));
	memcpy(tomatch, str.c_str(), strlen(str.c_str()));
	tomatch[str.length()] = 0;
	unsigned qlength = strlen(queryx) + 1;
	unsigned tlength = strlen(tomatch) + 1;
	// length filter
	if (abs((double) ((int) qlength - (int) tlength)) > (int) edThreshold)
		return FAILURE;
	unsigned ** flag = new unsigned *[qlength];
	// calc edit distance
	unsigned i = 0, j = 0;
	for (i = 0; i < qlength; i++) {
		flag[i] = new unsigned[tlength];
	}
	for (i = 1; i < qlength; i++)
		for (j = 1; j < tlength; j++)
			flag[i][j] = 0;
	for (i = 0; i < qlength; i++)
		flag[i][0] = i;
	for (j = 0; j < tlength; j++)
		flag[0][j] = j;
	unsigned t = 0;
	for (i = 1; i < qlength; i++)
		for (j = 1; j < tlength; j++) {
			if (queryx[i - 1] == tomatch[j - 1])
				t = 0;
			else
				t = 1;
			t += flag[i - 1][j - 1];
			if (flag[i - 1][j] + 1 < t)
				t = flag[i - 1][j] + 1;
			if (flag[i][j - 1] + 1 < t)
				t = flag[i][j - 1] + 1;
			flag[i][j] = t;
		}
	// check if word[num] is a result
	if (flag[qlength - 1][tlength - 1] > edThreshold) {
		for (i = 0; i < qlength; i++)
			delete[] flag[i];
		delete[] flag;
		return FAILURE;
	}
	for (i = 0; i < qlength; i++)
		delete[] flag[i];
	delete[] flag;
	return SUCCESS;
}

typedef struct {
	vector<vector<unsigned>*> *sublist;
	unsigned *count;
} SCargs;

pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

void *scancount(void *x) {
	pthread_mutex_lock(&count_mutex);
	SCargs args;
	memcpy(&args, x, sizeof(args));
	unsigned sublistsize = args.sublist->size();
	for (unsigned i = 0; i < sublistsize; i++) {
		unsigned sublist_j_size = args.sublist->at(i)->size();
		for (unsigned j = 0; j < sublist_j_size; j++) {
			args.count[args.sublist->at(i)->at(j)]++;
		}
	}
	pthread_mutex_unlock(&count_mutex);
	pthread_exit(NULL);
}

void FZ_Indexer::ScanCount(vector<ID_List *> lists, int T,
		vector<unsigned> &result, unsigned size) {
	//	cout << "ScanCount "<< size <<endl;
	result.clear();

#ifndef MULTITHREAD
	unsigned *count = new unsigned[size];
	//single-thread
	for (unsigned i = 0; i < size; i++)
		count[i] = 0;
	//		memset(count, 0, sizeof(unsigned) * size);

	for (unsigned i = 0; i < lists.size(); i++) {
		for (unsigned j = 0; j < lists[i]->size(); j++) {
			count[lists[i]->at(j)]++;
		}
	}
	for (unsigned i = 0; i < size; i++) {
		if ((int) (count[i]) >= T)
			result.push_back(i);
	}
	delete[] count;
#else

	//multi-thread
	unsigned *count = new unsigned[size];
	unsigned threads = THREADS;
	SCargs *args = new SCargs[threads];

	for (unsigned i = 0; i < threads; i++) {
		vector<vector<unsigned> *> *sublist = new vector<vector<unsigned> *> ();
		int factor = lists.size() / threads;
		for (unsigned j = factor * i; j < factor * (i + 1); j++)
		sublist->push_back(lists.at(j));
		if (i == threads - 1)
		for (unsigned j = factor * (i + 1); j < lists.size(); j++)
		sublist->push_back(lists.at(j));
		args[i].sublist = sublist;
		args[i].count = count;
	}
	pthread_t handles[threads];
	for (unsigned i = 0; i < threads; i++)
	pthread_create(&handles[i], NULL, scancount, args + i);
	for (unsigned i = 0; i < threads; i++)
	pthread_join(handles[i], NULL);
	for (unsigned i = 0; i < size; i++) {
		if ((int) (count[i]) >= T)
		result.push_back(i);
	}
	delete[] count;
#endif

}
