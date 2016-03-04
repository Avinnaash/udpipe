// This file is part of UDPipe <http://github.com/ufal/udpipe/>.
//
// Copyright 2015 Institute of Formal and Applied Linguistics, Faculty of
// Mathematics and Physics, Charles University in Prague, Czech Republic.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "trainer.h"
#include "trainer_morphodita_parsito.h"

namespace ufal {
namespace udpipe {

bool trainer::train(const string& method, const string& data, const string& tokenizer, const string& tagger, const string& parser, ostream& os, string& error) {
  error.clear();

  if (method == "morphodita_parsito") {
    os.put(method.size());
    os.write(method.c_str(), method.size());
    return trainer_morphodita_parsito::train(data, tokenizer, tagger, parser, os, error);
  }

  error.assign("Unknown UDPipe method '").append(method).append("'!");
  return false;
}

} // namespace udpipe
} // namespace ufal