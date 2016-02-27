// This file is part of UDPipe <http://github.com/ufal/udpipe/>.
//
// Copyright 2015 Institute of Formal and Applied Linguistics, Faculty of
// Mathematics and Physics, Charles University in Prague, Czech Republic.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "common.h"
#include "model.h"
#include "sentence/output_format.h"

namespace ufal {
namespace udpipe {

class pipeline {
 public:
  pipeline(const model* m, const string& tokenizer, const string& tagger, const string& parser);

  void set_model(const model* m);
  void set_tokenizer(const string& tokenizer);
  void set_tagger(const string& tagger);
  void set_parser(const string& parser);

  bool process(const string& input, ostream& os, string& error) const;

 private:
  bool process_tokenized(sentence& s, ostream& os, string& error) const;

  const model* m;
  string tokenizer, tagger, parser;
  unique_ptr<output_format> conllu_output;
};

} // namespace udpipe
} // namespace ufal
