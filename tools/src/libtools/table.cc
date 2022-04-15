/*
 * Copyright 2009-2020 The VOTCA Development Team (http://www.votca.org)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

// Standard includes
#include <fstream>
#include <stdexcept>
#include <vector>

// Third party includes
#include <boost/algorithm/string/replace.hpp>
#include <boost/range/algorithm.hpp>

// Local VOTCA includes
#include "votca/tools/lexical_cast.h"
#include "votca/tools/table.h"
#include "votca/tools/tokenizer.h"

namespace votca {
namespace tools {

using namespace boost;
using namespace std;

void Table::resize(Index N) {
  x_.conservativeResize(N);
  y_.conservativeResize(N);
  flags_.resize(N);
  if (has_yerr_) {
    yerr_.conservativeResize(N);
  }
}

void Table::Load(string filename) {
  ifstream in;
  in.open(filename);
  if (!in) {
    throw runtime_error(string("error, cannot open file ") + filename);
  }

  setErrorDetails("file " + filename);
  in >> *this;
  in.close();
}

void Table::Save(string filename) const {
  ofstream out;
  out.open(filename);
  if (!out) {
    throw runtime_error(string("error, cannot open file ") + filename);
  }

  if (has_comment_) {
    string str = "# " + comment_line_;
    boost::replace_all(str, "\n", "\n# ");
    boost::replace_all(str, "\\n", "\n# ");
    out << str << endl;
  }
  out << (*this);

  out.close();
}

void Table::clear(void) {
  x_.resize(0);
  y_.resize(0);
  flags_.resize(0);
  yerr_.resize(0);
}

double Table::getMaxY() const { return y_.maxCoeff(); }

double Table::getMinY() const { return y_.minCoeff(); }

double Table::getMaxX() const { return x_.maxCoeff(); }

double Table::getMinX() const { return x_.minCoeff(); }

// TODO: this functon is weired, reading occours twice, cleanup!!
// TODO: modify function to work properly, when  has_yerr_ is true
istream &operator>>(istream &in, Table &t) {
  size_t N = 0;
  bool bHasN = false;
  string line;
  Index line_number = 0;
  t.clear();

  // read till the first data line
  while (tools::getline(in, line)) {
    line_number++;
    string conversion_error = t.getErrorDetails() + ", line " +
                              boost::lexical_cast<string>(line_number);

    // remove comments and xmgrace stuff
    line = line.substr(0, line.find("#"));
    line = line.substr(0, line.find("@"));

    // tokenize string and put it to vector
    vector<string> tokens = Tokenizer(line, " \t").ToVector();

    // skip empty lines
    if (tokens.size() == 0) {
      continue;
    }

    // if first line is only 1 token, it's the size
    if (tokens.size() == 1) {
      N = lexical_cast<Index>(tokens[0], conversion_error);
      bHasN = true;
    } else if (tokens.size() == 2) {
      // it's the first data line with 2 or 3 entries
      t.push_back(std::stod(tokens[0]), std::stod(tokens[1]), 'i');
    } else if (tokens.size() > 2) {
      char flag = 'i';
      string sflag = tokens.back();
      if (sflag == "i" || sflag == "o" || sflag == "u") {
        flag = sflag[0];
      }
      t.push_back(std::stod(tokens[0]), std::stod(tokens[1]), flag);
    } else {
      throw runtime_error("error, wrong table format");
    }
  }

  // read the rest
  while (tools::getline(in, line)) {
    line_number++;
    string conversion_error = t.getErrorDetails() + ", line " +
                              boost::lexical_cast<string>(line_number);

    // remove comments and xmgrace stuff
    line = line.substr(0, line.find("#"));
    line = line.substr(0, line.find("@"));

    // tokenize string and put it to vector
    vector<string> tokens = Tokenizer(line, " \t").ToVector();

    // skip empty lines
    if (tokens.size() == 0) {
      continue;
    }

    // it's a data line
    if (tokens.size() == 2) {
      t.push_back(std::stod(tokens[0]), std::stod(tokens[1]), 'i');
    } else if (tokens.size() > 2) {
      char flag = 'i';
      if (tokens[2] == "i" || tokens[2] == "o" || tokens[2] == "u") {
        flag = tokens[2][0];
      }
      t.push_back(std::stod(tokens[0]), std::stod(tokens[1]), flag);
    } else {
      // otherwise error
      throw runtime_error("error, wrong table format");
    }
    // was size given and did we read N values?
    if (bHasN) {
      if (--N == 0) {
        break;
      }
    }
  }

  return in;
}

void Table::GenerateGridSpacing(double min, double max, double spacing) {
  Index vec_size = (Index)((max - min) / spacing + 1.00000001);
  resize(vec_size);
  // adjust spacing to generate evenly distributed bins till max
  double spacing_scaled = (max - min) / (double(vec_size) - 1.0);

  double r_init;
  int i;
  for (r_init = min, i = 0; i < vec_size - 1; r_init += spacing_scaled) {
    x_[i++] = r_init;
  }
  x_[i] = max;
}

void Table::Smooth(Index Nsmooth) {

  Index n_2 = size() - 2;
  if (n_2 < 0) {
    throw std::runtime_error(
        "Smoothing only works for arrays of size 3 and larger");
  }
  for (Index i = 0; i < Nsmooth; i++) {
    y_.segment(1, n_2) =
        (0.25 * (y_.head(n_2) + 2 * y_.segment(1, n_2) + y_.tail(n_2))).eval();
  }
}

std::ostream &operator<<(std::ostream &out, const Table &t) {
  // TODO: use a smarter precision guess, XXX.YYYYY=8, so 10 should be enough
  out.precision(10);

  if (t.has_yerr_) {
    for (Index i = 0; i < t.x_.size(); ++i) {
      out << t.x_[i] << " " << t.y_[i] << " " << t.yerr_[i];
      if (t.flags_[i] == '\0' || t.flags_[i] == ' ') {
        out << "\n";
      } else {
        out << " " << t.flags_[i] << "\n";
      }
    }
  } else {
    for (Index i = 0; i < t.x_.size(); ++i) {
      out << t.x_[i] << " " << t.y_[i];
      if (t.flags_[i] == '\0' || t.flags_[i] == ' ') {
        out << "\n";
      } else {
        out << " " << t.flags_[i] << "\n";
      }
    }
  }
  out << std::flush;
  return out;
}

// TODO: modify this function to be able to treat  has_yerr_ == true
void Table::push_back(double x, double y, char flags) {
  Index n = size();
  resize(n + 1);
  x_[n] = x;
  y_[n] = y;
  flags_[n] = flags;
}
}  // namespace tools
}  // namespace votca
