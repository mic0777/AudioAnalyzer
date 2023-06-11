#include "worker.h"
#include <list>
#include <numeric>
#include <cstring>
#include "keyfinder/keyfinder.h"
#include "aubio/aubio.h"
#include "aubio/tempo/beattracking.h"


using namespace std;
extern tuple<vector<float>, int64_t, unsigned>  decodeAudio(vector<char> &compressedBuf);

void Worker::operator()()
{
    static mutex m;
    try{
        auto [waveData, dur, freq]  = decodeAudio(compressedAudio);
        duration = dur / 1000000;
        frequency = freq;
        string key = detectKey(waveData);
        string tempo=detectTempo(waveData);

        unique_lock<mutex> lock(m);
        writeToCSV(key, tempo);
    }
    catch(exception &e) {
        unique_lock<mutex> lock(m);
        ofstream f("bad.txt", ios::app);
        f << songName << ": " << e.what() << "\n";
    }
}

string Worker::detectTempo(std::vector<float> &wav)
{
    uint_t win_s = 1024; // window size
    std::shared_ptr<fvec_t> in(new_fvec (win_s), &del_fvec); // input buffer
    std::shared_ptr<fvec_t> out(new_fvec (win_s / 4), &del_fvec); // output beat position

    // create beattracking object
    //aubio_beattracking_t * tempo  = new_aubio_beattracking(win_s, 256, 44100);
    std::shared_ptr<aubio_tempo_t> o(new_aubio_tempo("specdiff", win_s, 512, 44100), &del_aubio_tempo);

    smpl_t bpm, confidence;
    list<smpl_t> beats;
    list<smpl_t> bpms_;
    list<smpl_t> conf;
    for (int i=0; i < (wav.size())/win_s; ++i) {
      memcpy(in->data, wav.data()+i*win_s, win_s*4);

      // execute tempo
      aubio_tempo_do(o.get(), in.get(), out.get());
      // do something with the beats
      if (out->data[0] != 0 /*&&  aubio_tempo_get_confidence(o)>0.4*/) {
          beats.push_back(aubio_tempo_get_last_s(o.get()));
          conf.push_back(aubio_tempo_get_confidence(o.get()));
          bpms_.push_back(aubio_tempo_get_bpm(o.get()));
        //printf("beat at %.3fms, %.3fs, frame %d, %.2fbpm with confidence %.2f\n",
            //aubio_tempo_get_last_ms(o), aubio_tempo_get_last_s(o),
            //aubio_tempo_get_last(o), aubio_tempo_get_bpm(o), aubio_tempo_get_confidence(o));
      }

    }
    smpl_t avgConf=0;
    smpl_t bestConf=0;
    smpl_t bestBPM=0;
    for (auto it1=conf.begin(), it2=bpms_.begin(); it1!=conf.end(); ++it1, ++it2) {
        avgConf+= *it1;
        if (bestConf  < *it1)
        {
            bestConf = *it1;
            bestBPM = *it2;
        }
    }
    if (conf.size()>0) avgConf /= conf.size();

    // filter best 50%:
    smpl_t filterLevel = avgConf;
    for (auto it=conf.begin(), itBeats=beats.begin(); it!=conf.end(); ++it, ++itBeats) {
        if (*it < filterLevel) {
            // delete it
            it = conf.erase(it);
            itBeats = beats.erase(itBeats);
        }
    }

    string strResult;
    //def beats_to_bpm(beats, path):
    adjacent_difference(beats.begin(), beats.end(), beats.begin());
    beats.pop_front();  // first element produced by adjacent_difference is not difference
           // if enough beats are found, convert to periods then to bpm
           if (beats.size() > 1) {
               list<smpl_t> bpms;
               for (auto it=beats.begin(); it!=beats.end(); ++it) {
                   if (*it > 0)
                      bpms.push_back(60.0 / *it);
               }

               strResult =  to_string(bestBPM);
           }else{
               throw std::runtime_error("not enough beats found");
               }

    // called from shared_ptr:
    // del_aubio_tempo(o);
    // del_fvec(in);
    // del_fvec(out);
    // aubio_cleanup();  <-- called when all files are done
    return strResult;

}
string Worker::detectKey(std::vector<float> &wavData)
{
    const size_t sampleSize{2}; // only 16 bit supported
    const size_t sampleCount = wavData.size() / sampleSize;

    // Build an empty audio object
    KeyFinder::AudioData a;
    // Prepare the object for audio stream
    a.setFrameRate(frequency);
    a.setChannels(1);
    a.addToSampleCount(sampleCount);
    if (sampleCount <= 0) {
        throw std::runtime_error("no samples found!");
    }

    // Populate the KeyFinder::AudioData object with the samples
    a.resetIterators();
    for (unsigned int i = 0; i < sampleCount; ++i) {
        a.setSampleAtWriteIterator(*(wavData.data() + i));
        //a.setSampleAtWriteIterator(float(*(reinterpret_cast<float*>(wavData.data()) + i)));
        a.advanceWriteIterator();
    }


    // Static because it retains useful resources for repeat use
     KeyFinder::KeyFinder k;


     // Run the analysis
     KeyFinder::key_t key = k.keyOfAudio(a);

     switch (key)
     {
     case KeyFinder::A_MAJOR:       return "A";
     case KeyFinder::A_MINOR:       return "Am";
     case KeyFinder::B_FLAT_MAJOR:  return "Bb";
     case KeyFinder::B_FLAT_MINOR:  return "Bbm";
     case KeyFinder::B_MAJOR:       return "B";
     case KeyFinder::B_MINOR:       return "Bm";
     case KeyFinder::C_MAJOR:       return "C";
     case KeyFinder::C_MINOR:       return "Cm";
     case KeyFinder::D_FLAT_MAJOR:  return "Db";
     case KeyFinder::D_FLAT_MINOR:  return "C#m";
     case KeyFinder::D_MAJOR:       return "D";
     case KeyFinder::D_MINOR:       return "Dm";
     case KeyFinder::E_FLAT_MAJOR:  return "Eb";
     case KeyFinder::E_FLAT_MINOR:  return "Ebm";
     case KeyFinder::E_MAJOR:       return "E";
     case KeyFinder::E_MINOR:       return "Em";
     case KeyFinder::F_MAJOR:       return "F";
     case KeyFinder::F_MINOR:       return "Fm";
     case KeyFinder::G_FLAT_MAJOR:  return "Gb";
     case KeyFinder::G_FLAT_MINOR:  return "F#m";
     case KeyFinder::G_MAJOR:       return "G";
     case KeyFinder::G_MINOR:       return "Gm";
     case KeyFinder::A_FLAT_MAJOR:  return "Ab";
     case KeyFinder::A_FLAT_MINOR:  return "G#m";
     default: return "";
     }



     return "";
}


void Worker::writeToCSV(string &key, string &tempo)
{
    ofStream << songName << "," << duration << "," << frequency << key << "," << tempo << endl;
}
