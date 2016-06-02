// This file is part of UDPipe <http://github.com/ufal/udpipe/>.
//
// Copyright 2015 Institute of Formal and Applied Linguistics, Faculty of
// Mathematics and Physics, Charles University in Prague, Czech Republic.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "evaluator.h"
#include "sentence/input_format.h"
#include "unilib/utf8.h"

namespace ufal {
namespace udpipe {

const string evaluator::DEFAULT;
const string evaluator::NONE = "none";

evaluator::evaluator(const model* m, const string& tokenizer, const string& tagger, const string& parser) {
  set_model(m);
  set_tokenizer(tokenizer);
  set_tagger(tagger);
  set_parser(parser);
}

void evaluator::set_model(const model* m) {
  this->m = m;
}

void evaluator::set_tokenizer(const string& tokenizer) {
  this->tokenizer = tokenizer;
}

void evaluator::set_tagger(const string& tagger) {
  this->tagger = tagger;
}

void evaluator::set_parser(const string& parser) {
  this->parser = parser;
}

bool evaluator::evaluate(istream& is, ostream& os, string& error) const {
  class tokenizer_evaluator {
   public:
    void add_sentence(const sentence& s) {
      sentences.emplace_back(chars.size(), chars.size());
      for (size_t i = 1, j = 0; i < s.words.size(); i++) {
        tokens.emplace_back(chars.size(), chars.size());
        const string& form = j < s.multiword_tokens.size() && s.multiword_tokens[j].id_first == int(i) ? s.multiword_tokens[j].form : s.words[i].form;
        for (auto&& chr : unilib::utf8::decoder(form))
          chars.push_back(chr);
        tokens.back().second = chars.size();

        if (j < s.multiword_tokens.size() && s.multiword_tokens[j].id_first == int(i)) {
          multiwords.emplace_back(tokens.back().first, form);
          multiwords.back().second.append(" ").append(s.words[i].form);
          while (int(i) < s.multiword_tokens[j].id_last)
            multiwords.back().second.append(" ").append(s.words[++i].form);
          j++;
        }
      }
      sentences.back().second = chars.size();
    }

    bool can_evaluate(const tokenizer_evaluator& gold) { return chars == gold.chars; }
    f1_info evaluate_sentences(const tokenizer_evaluator& gold) { return evaluate_f1(sentences, gold.sentences); }
    f1_info evaluate_tokens(const tokenizer_evaluator& gold) { return evaluate_f1(tokens, gold.tokens); }
    f1_info evaluate_multiwords(const tokenizer_evaluator& gold) { return evaluate_f1(multiwords, gold.multiwords); }

   private:
    u32string chars;
    vector<pair<size_t, size_t>> sentences, tokens;
    vector<pair<size_t, string>> multiwords;
  } system_tokenizer, gold_tokenizer;

  error.clear();

  unique_ptr<input_format> conllu_input(input_format::new_conllu_input_format());
  if (!conllu_input) return error.assign("Cannot allocate CoNLL-U input format instance!"), false;

  string plain_text; unsigned space_after_nos = 0;
  sentence system, gold;
  int words = 0, upostag = 0, xpostag = 0, feats = 0, all_tags = 0, lemma = 0;
  int edges = 0, uas = 0, las = 0;

  string block;
  while (conllu_input->read_block(is, block)) {
    conllu_input->set_text(block);
    while (conllu_input->next_sentence(gold, error)) {
      // Detokenize the input when tokenizing
      if (tokenizer != NONE) {
        gold_tokenizer.add_sentence(gold);

        bool previous_nospace = true;
        for (size_t i = 1, j = 0; i < gold.words.size(); i++) {
          if (!previous_nospace) plain_text.push_back(' ');
          plain_text.append(j < gold.multiword_tokens.size() && gold.multiword_tokens[j].id_first == int(i) ? gold.multiword_tokens[j].form : gold.words[i].form);
          const string& misc = j < gold.multiword_tokens.size() && gold.multiword_tokens[j].id_first == int(i) ? gold.multiword_tokens[j].misc : gold.words[i].misc;
          previous_nospace = misc.find(space_after_no) != string::npos;
          space_after_nos += previous_nospace ? 1 : 0;
          if (j < gold.multiword_tokens.size() && gold.multiword_tokens[j].id_first == int(i))
            i = gold.multiword_tokens[j++].id_last;
        }
        plain_text.push_back(' ');
      }

      // Create empty copy
      system.clear();
      for (size_t i = 1; i < gold.words.size(); i++)
        system.add_word(gold.words[i].form);

      // Tag
      if (tagger != NONE) {
        if (!m->tag(system, tagger, error))
          return false;
        for (size_t i = 1; i < gold.words.size(); i++) {
          words++;
          upostag += gold.words[i].upostag == system.words[i].upostag;
          xpostag += gold.words[i].xpostag == system.words[i].xpostag;
          feats += gold.words[i].feats == system.words[i].feats;
          all_tags += gold.words[i].upostag == system.words[i].upostag && gold.words[i].xpostag == system.words[i].xpostag && gold.words[i].feats == system.words[i].feats;
          lemma += gold.words[i].lemma == system.words[i].lemma;
        }
      } else {
        for (size_t i = 1; i < gold.words.size(); i++) {
          system.words[i].upostag = gold.words[i].upostag;
          system.words[i].xpostag = gold.words[i].xpostag;
          system.words[i].feats = gold.words[i].feats;
          system.words[i].lemma = gold.words[i].lemma;
        }
      }

      // Parse
      if (parser != NONE) {
        if (!m->parse(system, parser, error))
          return false;
        for (size_t i = 1; i < gold.words.size(); i++) {
          edges++;
          uas += gold.words[i].head == system.words[i].head;
          las += gold.words[i].head == system.words[i].head && gold.words[i].deprel == system.words[i].deprel;
        }
      }
    }
    if (!error.empty()) return false;
  }

  // Tokenize the input and evaluate
  if (tokenizer != NONE) {
    unique_ptr<input_format> t(m->new_tokenizer(tokenizer));
    if (!t) return error.assign("Cannot allocate new tokenizer!"), false;

    t->set_text(plain_text);
    while (t->next_sentence(system, error))
      system_tokenizer.add_sentence(system);
    if (!error.empty()) return false;

    // Evaluate
    if (!system_tokenizer.can_evaluate(gold_tokenizer)) {
      os << "Cannot evaluate tokenizer, it returned different sequence of token characters!" << endl;
    } else {
      auto tokens = system_tokenizer.evaluate_tokens(gold_tokenizer);
      auto sentences = system_tokenizer.evaluate_sentences(gold_tokenizer);
      auto multiwords = system_tokenizer.evaluate_multiwords(gold_tokenizer);
      if (!space_after_nos) {
        os << "No SpaceAfter=No features, not evaluating tokenizer performance." << endl;
      } else {
        os << "Tokenizer - tokens system: " << tokens.total_system << ", gold: " << tokens.total_gold
           << ", precision: " << fixed << setprecision(2) << 100. * tokens.precision
           << "%, recall: " << 100. * tokens.recall << "%, f1: " << 100. * tokens.f1 << "%" << endl;
        os << "Tokenizer - sentences system: " << sentences.total_system << ", gold: " << sentences.total_gold
           << ", precision: " << fixed << setprecision(2) << 100. * sentences.precision
           << "%, recall: " << 100. * sentences.recall << "%, f1: " << 100. * sentences.f1 << "%" << endl;
      }
      if (multiwords.total_gold || multiwords.total_system)
        os << "Tokenizer - multiwords system: " << multiwords.total_system << " (" << 100. * multiwords.total_system / tokens.total_system
           << "%), gold: " << multiwords.total_gold << " (" << 100. * multiwords.total_gold / tokens.total_gold
           << "%), precision: " << fixed << setprecision(2) << 100. * multiwords.precision
           << "%, recall: " << 100. * multiwords.recall << "%, f1: " << 100. * multiwords.f1 << "%" << endl;
    }
  }

  if (tagger != NONE)
    os << "Tagging from gold tokenization - forms: " << words << ", upostag: " << fixed << setprecision(2) << 100. * upostag / words << "%, xpostag: " << 100. * xpostag / words
       << "%, feats: " << 100. * feats / words << "%, all tags: " << 100. * all_tags / words << "%, lemma: " << 100. * lemma / words << '%' << endl;
  if (parser != NONE)
    os << "Parsing from gold tokenization - UAS: " << fixed << setprecision(2) << 100. * uas / edges << "%, LAS: " << 100. * las / edges << '%' << endl;

  return true;
}

template <class T>
evaluator::f1_info evaluator::evaluate_f1(const vector<pair<size_t, T>>& system, const vector<pair<size_t, T>>& gold) {
  size_t both = 0;
  for (size_t si = 0, gi = 0; si < system.size() || gi < gold.size(); )
    if (si < system.size() && (gi == gold.size() || system[si].first < gold[gi].first))
      si++;
    else if (gi < gold.size() && (si == system.size() || gold[gi].first < system[si].first))
      gi++;
    else
      both += system[si++].second == gold[gi++].second;

  return {system.size(), gold.size(), system.size() ? both / double(system.size()) : 0.,
    gold.size() ? both / double(gold.size()) : 0., system.size()+gold.size() ? 2 * both / double(system.size() + gold.size()) : 0. };
}

const string evaluator::space_after_no = "SpaceAfter=No";

} // namespace udpipe
} // namespace ufal
