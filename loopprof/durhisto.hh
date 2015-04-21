#ifndef DUR_HISTO_HH
#define DUR_HISTO_HH

#include <vector>
#include <iomanip>
#include <map>


class HistoItem {
public:
  int size;

  std::vector<uint64_t> histo;
  std::vector<uint64_t> cdf_histo;

  LoopInfo* li=NULL; //potentially useful
  uint64_t start=0;    //used for loops

  void calcHisto(int num_elements) {
    uint64_t cur=0;
    for(int i = num_elements-1; i>=0; --i) {
      cur+=histo[i];
      cdf_histo.push_back(cur);
    }
  }

};


class DurationHisto {
public:

  uint64_t duration; //used for traces
  uint64_t start;    //used for loops

  static std::vector<int> sizes; 

  std::map<int,HistoItem> histo_map;
  std::map<int,HistoItem> histo_map_i;

  static const int num_divisions_per_power_of_2=4;
  static const int max_duration=30;

  static int num_elements() {
    return max_duration*num_divisions_per_power_of_2;
  }

  DurationHisto() {
    for(auto i : sizes) {
      histo_map[i].size=i;
      histo_map[i].histo.resize(num_elements());
      histo_map_i[i].size=i;
      histo_map_i[i].histo.resize(num_elements());
    }
  }

  int ind_of_duration(uint64_t duration) {
    double dur = duration;
    double log_dur = log2(dur);
    double adj_log_dur = log_dur * num_divisions_per_power_of_2;
    int ind = (int) adj_log_dur;
    assert(ind < num_elements());
    return ind;
  }

  //size is size of static instructions
  void addHist(uint64_t duration, int size, bool needs_inlining) {
    int ind = ind_of_duration(duration);

    if(!needs_inlining) {
      for(auto& i : histo_map) {
        HistoItem& item= i.second;
        if(size<=item.size) {
          item.histo[ind]+=duration;
        }
      }
    }
    for(auto& i : histo_map_i) {
      HistoItem& item= i.second;
      if(size<=item.size) {
        item.histo[ind]+=duration;
      }
    }

  }

  // hist_size is size of hist to update
  void addSpecificHist(uint64_t duration, HistoItem& item) {
    int ind = ind_of_duration(duration);
    item.histo[ind]+=duration;
  }

  void printCDF(std::ostream& out, int pre, double scaling_factor) {
    for(auto& i : histo_map) {
      HistoItem& item= i.second;

      if(item.cdf_histo.size()==0) {
        item.calcHisto(num_elements());
      }
      out << pre << "," << item.size << ":";
      for(int i = 0; (unsigned)i < item.cdf_histo.size(); ++i) {
        out << std::setprecision(4) << item.cdf_histo[i] * scaling_factor<< " ";
      }
      out << "\n";
    }
    for(auto& i : histo_map_i) {
      HistoItem& item= i.second;

      if(item.cdf_histo.size()==0) {
        item.calcHisto(num_elements());
      }
      out << pre << "," << item.size << ":";
      for(int i = 0; (unsigned)i < item.cdf_histo.size(); ++i) {
        out << std::setprecision(4) << item.cdf_histo[i] * scaling_factor<< " ";
      }
      out << "\n";
    }
  }

};

#endif
