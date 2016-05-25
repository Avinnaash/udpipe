// This file is part of UDPipe <http://github.com/ufal/udpipe/>.
//
// Copyright 2015 Institute of Formal and Applied Linguistics, Faculty of
// Mathematics and Physics, Charles University in Prague, Czech Republic.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <utility>

#include "common.h"
#include "model.h"
#include "sentence/output_format.h"

namespace ufal {
namespace udpipe {

class pipeline {
 public:
  pipeline(const model* m, const string& input_format, const string& tagger, const string& parser, const string& output_format);

  void set_model(const model* m);
  void set_input_format(const string& input_format);
  void set_tagger(const string& tagger);
  void set_parser(const string& parser);
  void set_output_format(const string& output_format);

  bool process(istream& is, ostream& os, string& error) const;
  bool evaluate(istream& is, ostream& os, string& error) const;

  static const string DEFAULT;
  static const string NONE;

 private:
  const model* m;
  string input_format_desc, tokenizer, tagger, parser, output_format_desc;

  struct f1_info { size_t total_system, total_gold; double precision, recall, f1; };
  template <class T>
  static f1_info evaluate_f1(const vector<pair<size_t, T>>& system, const vector<pair<size_t, T>>& gold);

  static const string space_after_no;
};

} // namespace udpipe
} // namespace ufal
