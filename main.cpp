//  Created by Michael Philatov
//  Copyright © 2023. All rights reserved.
//
#include <iostream>
#include <filesystem>
#include <fstream>
#include <chrono>
#include "worker.h"

using namespace std;

int main(int argc, char**argv) {
    if (argc != 3) {
        cout << "Wrong number of params. Use: AudioAnalyzer <folder with audio files> <result CSV file path>" << endl;
        return 0;
    }
    string path(argv[1]);
    string csvPath(argv[2]);
    ofstream resultCSV(csvPath);
    resultCSV << "File Name,Duration,Frequency,Key,Tempo" << endl;
    
    try{
        ThreadPool pool{thread::hardware_concurrency() - 1};
        for (const auto & entry : filesystem::directory_iterator(path)) {
            string src{entry.path()}, name{entry.path().stem()};
            cout << src << endl;
            auto filesize = filesystem::file_size(src);
            if (filesize <= 0) throw(new invalid_argument("file is empty: " + name));
            ifstream f(src, ios::binary);
            vector<char> buf;
            buf.resize(filesize);
            f.read(buf.data(), filesize);
            //auto w = new Worker(move(buf), resultCSV, name);
            pool.submit(Worker(move(buf), resultCSV, name));            
        }
        while(!pool.done()) {
            int percent{pool.getPercentDone()};
            cout << percent << "%\r" << flush;
            if (percent == 100) break;
            this_thread::sleep_for(1s);
        }
        cout << pool.getTotalDone() << " file(s) processed\n";
        if (pool.exception) {
            std::rethrow_exception(pool.exception);
        }
    }
    catch(exception& ex) {
        cout << ex.what() << endl;
    }

    return 0;
}
