// This file is part of UDPipe <http://github.com/ufal/udpipe/>.
//
// Copyright 2015 Institute of Formal and Applied Linguistics, Faculty of
// Mathematics and Physics, Charles University in Prague, Czech Republic.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>

#include "model_morphodita_parsito.h"
#include "unilib/utf8.h"
#include "utils/getpara.h"
#include "utils/parse_int.h"

namespace ufal {
namespace udpipe {

// Versions:
// 1 - initial version
// 2 - add absolute lemmas (tagger_model::lemma == 2)
//   - use Arabic and space normalization

input_format* model_morphodita_parsito::new_tokenizer(const string& options) const {
  named_values::map parsed_options;
  string parse_error;
  if (!named_values::parse(options, parsed_options, parse_error))
    return nullptr;

  bool normalized_spaces = parsed_options.count("normalized_spaces") && parsed_options["normalized_spaces"] != "0";

  input_format* result = tokenizer_factory ? new tokenizer_morphodita(tokenizer_factory->new_tokenizer(), *splitter.get(), normalized_spaces) : nullptr;

  return (parsed_options.count("presegmented") && parsed_options["presegmented"] != "0") ? input_format::new_presegmented_tokenizer(result) : result;
}

bool model_morphodita_parsito::tag(sentence& s, const string& /*options*/, string& error) const {
  error.clear();

  if (taggers.empty()) return error.assign("No tagger defined for the UDPipe model!"), false;
  if (s.empty()) return true;

  tagger_cache* c = tagger_caches.pop();
  if (!c) c = new tagger_cache();

  // Prepare input forms
  c->forms_normalized.resize(s.words.size() - 1);
  c->forms_string_pieces.resize(s.words.size() - 1);
  for (size_t i = 1; i < s.words.size(); i++)
    c->forms_string_pieces[i - 1] = normalize_form(s.words[i].form, c->forms_normalized[i - 1], true);

  // Clear first
  for (size_t i = 1; i < s.words.size(); i++) {
    s.words[i].lemma.assign("_");
    s.words[i].upostag.clear();
    s.words[i].xpostag.clear();
    s.words[i].feats.clear();
  }

  // Fill information from the tagger models
  for (auto&& tagger : taggers) {
    if (!tagger.tagger) return error.assign("No tagger defined for the UDPipe model!"), false;

    tagger.tagger->tag(c->forms_string_pieces, c->lemmas);

    for (size_t i = 0; i < c->lemmas.size(); i++)
      fill_word_analysis(c->lemmas[i], tagger.upostag, tagger.lemma, tagger.xpostag, tagger.feats, s.words[i+1]);
  }

  tagger_caches.push(c);
  return true;
}

bool model_morphodita_parsito::parse(sentence& s, const string& options, string& error) const {
  error.clear();

  if (!parser) return error.assign("No parser defined for the UDPipe model!"), false;
  if (s.empty()) return true;

  parser_cache* c = parser_caches.pop();
  if (!c) c = new parser_cache();

  int beam_search = 5;
  if (!named_values::parse(options, c->options, error))
    return false;
  if (c->options.count("beam_search"))
    if (!parse_int(c->options["beam_search"], "beam_search", beam_search, error))
      return false;

  c->tree.clear();
  for (size_t i = 1; i < s.words.size(); i++) {
    c->tree.add_node(string());
    normalize_form(s.words[i].form, c->tree.nodes.back().form, false);
    c->tree.nodes.back().lemma.assign(s.words[i].lemma);
    c->tree.nodes.back().upostag.assign(s.words[i].upostag);
    c->tree.nodes.back().xpostag.assign(s.words[i].xpostag);
    c->tree.nodes.back().feats.assign(s.words[i].feats);
    c->tree.nodes.back().deps.assign(s.words[i].deps);
    c->tree.nodes.back().misc.assign(s.words[i].misc);
  }

  parser->parse(c->tree, beam_search);
  for (size_t i = 1; i < s.words.size(); i++)
    s.set_head(i, c->tree.nodes[i].head, c->tree.nodes[i].deprel);

  parser_caches.push(c);
  return true;
}

model* model_morphodita_parsito::load(istream& is) {
  char version;
  if (!is.get(version)) return nullptr;
  if (!(version >= 1 && version <= VERSION_LATEST)) return nullptr;

  // Because UDPipe 1.0 does not check the model version,
  // a specific sentinel was added since version 2 so that
  // loading of such model fail on UDPipe 1.0
  if (version >= 2) {
    char sentinel;
    if (!is.get(sentinel) || sentinel != 0x7F) return nullptr;
    if (!is.get(sentinel) || sentinel != 0x7F) return nullptr;
  }

  unique_ptr<model_morphodita_parsito> m(new model_morphodita_parsito((unsigned char)version));
  if (!m) return nullptr;

  char tokenizer;
  if (!is.get(tokenizer)) return nullptr;
  m->tokenizer_factory.reset(tokenizer ? morphodita::tokenizer_factory::load(is) : nullptr);
  if (tokenizer && !m->tokenizer_factory) return nullptr;
  m->splitter.reset(tokenizer ? multiword_splitter::load(is) : nullptr);
  if (tokenizer && !m->splitter) return nullptr;

  m->taggers.clear();
  char taggers; if (!is.get(taggers)) return nullptr;
  for (char i = 0; i < taggers; i++) {
    char lemma; if (!is.get(lemma)) return nullptr;
    char xpostag; if (!is.get(xpostag)) return nullptr;
    char feats; if (!is.get(feats)) return nullptr;
    morphodita::tagger* tagger = morphodita::tagger::load(is);
    if (!tagger) return nullptr;
    m->taggers.emplace_back(i == 0, int(lemma), bool(xpostag), bool(feats), tagger);
  }

  char parser;
  if (!is.get(parser)) return nullptr;
  m->parser.reset(parser ? parsito::parser::load(is) : nullptr);
  if (parser && !m->parser) return nullptr;

  return m.release();
}

model_morphodita_parsito::model_morphodita_parsito(unsigned version) : version(version) {}

model_morphodita_parsito::tokenizer_morphodita::tokenizer_morphodita(morphodita::tokenizer* tokenizer, const multiword_splitter& splitter, bool normalized_spaces)
  : tokenizer(tokenizer), splitter(splitter), normalized_spaces(normalized_spaces) {}

bool model_morphodita_parsito::tokenizer_morphodita::read_block(istream& is, string& block) const {
  return bool(getpara(is, block));
}

void model_morphodita_parsito::tokenizer_morphodita::set_text(string_piece text, bool make_copy) {
  tokenizer->set_text(text, make_copy);
}

bool model_morphodita_parsito::tokenizer_morphodita::next_sentence(sentence& s, string& error) {
  s.clear();
  error.clear();
  if (tokenizer->next_sentence(&forms, nullptr)) {
    for (size_t i = 0; i < forms.size(); i++) {
      if (normalized_spaces)
        token_with_spaces.set_space_after(!(i+1 < forms.size() && forms[i+1].str == forms[i].str + forms[i].len));
      else
        token_with_spaces.set_spaces_after(string_piece(forms[i].str + forms[i].len, i+1 < forms.size() ? forms[i+1].str - forms[i].str - forms[i].len : 0));
      splitter.append_token(forms[i], token_with_spaces.misc, s);
    }

    // Fill "# text" comment
    s.comments.emplace_back("# text =");
    for (unsigned i = 1; i < s.words.size(); i++) {
      if (i == 1 || s.words[i-1].get_space_after()) s.comments.back().push_back(' ');
      s.comments.back().append(s.words[i].form);
    }

    return true;
  }

  return false;
}

void model_morphodita_parsito::fill_word_analysis(const morphodita::tagged_lemma& analysis, bool upostag, int lemma, bool xpostag, bool feats, word& word) const {
  // Lemma
  if (lemma == 1) {
    word.lemma.assign(analysis.lemma);
  } else if (lemma == 2) {
    word.lemma.assign(analysis.lemma);

    // Lemma matching ~replacement~original_form is changed to replacement.
    if (analysis.lemma[0] == '~') {
      auto end = analysis.lemma.find('~', 1);
      if (end != string::npos && analysis.lemma.compare(end + 1, string::npos, word.form) == 0)
        word.lemma.assign(analysis.lemma, 1, end - 1);
    }
  }
  if (version >= 2) {
    // Replace '\001' back to spaces
    for (auto && chr : word.lemma)
      if (chr == '\001')
        chr = ' ';
  }

  if (!upostag && !xpostag && !feats) return;

  // UPOSTag
  char separator = analysis.tag[0];
  size_t start = min(size_t(1), analysis.tag.size()), end = min(analysis.tag.find(separator, 1), analysis.tag.size());
  if (upostag) word.upostag.assign(analysis.tag, start, end - start);

  if (!xpostag && !feats) return;

  // XPOSTag
  start = min(end + 1, analysis.tag.size());
  end = min(analysis.tag.find(separator, start), analysis.tag.size());
  if (xpostag) word.xpostag.assign(analysis.tag, start, end - start);

  if (!feats) return;

  // Features
  start = min(end + 1, analysis.tag.size());
  word.feats.assign(analysis.tag, start, analysis.tag.size() - start);
}

const string& model_morphodita_parsito::normalize_form(string_piece form, string& output, bool also_spaces) const {
  using unilib::utf8;

  // No normalization on version 1
  if (version <= 1) return output.assign(form.str, form.len);

  // If requested, replace space by \001 since version 2.

  // Arabic normalization since version 2, implementation resulted from
  // discussion with Otakar Smrz and Nasrin Taghizadeh.
  // 1. Remove https://codepoints.net/U+0640 without any reasonable doubt :)
  // 2. Remove https://codepoints.net/U+0652
  // 3. Remove https://codepoints.net/U+0670
  // 4. Remove everything from https://codepoints.net/U+0653 to
  //    https://codepoints.net/U+0657 though they are probably very rare in date
  // 5. Remove everything from https://codepoints.net/U+064B to
  //    https://codepoints.net/U+0650
  // 6. Remove https://codepoints.net/U+0651
  // 7. Replace https://codepoints.net/U+0671 with https://codepoints.net/U+0627
  // 8. Replace https://codepoints.net/U+0622 with https://codepoints.net/U+0627
  // 9. Replace https://codepoints.net/U+0623 with https://codepoints.net/U+0627
  // 10. Replace https://codepoints.net/U+0625 with https://codepoints.net/U+0627
  // 11. Replace https://codepoints.net/U+0624 with https://codepoints.net/U+0648
  // 12. Replace https://codepoints.net/U+0626 with https://codepoints.net/U+064A
  // One might also consider replacing some Farsi characters that might be typed
  // unintentionally (by Iranians writing Arabic language texts):
  // 13. Replace https://codepoints.net/U+06CC with https://codepoints.net/U+064A
  // 14. Replace https://codepoints.net/U+06A9 with https://codepoints.net/U+0643
  // 15. Replace https://codepoints.net/U+06AA with https://codepoints.net/U+0643
  //
  // Not implemented:
  // There is additional challenge with data coming from Egypt (such as printed
  // or online newspapers), where the word-final https://codepoints.net/U+064A
  // may be switched for https://codepoints.net/U+0649 and visa versa. Also, the
  // word-final https://codepoints.net/U+0647 could actually represent https://
  // codepoints.net/U+0629. You can experiment with the following replacements,
  // but I would rather apply them only after classifying the whole document as
  // following such convention:
  // 1. Replace https://codepoints.net/U+0629 with https://codepoints.net/U+0647
  //    (frequent femine ending markers would appear like a third-person
  //    masculine pronoun clitic instead)
  // 2. Replace https://codepoints.net/U+0649 with https://codepoints.net/U+064A
  //    (some "weak" words would become even more ambiguous or appear as if
  //    with a first-person pronoun clitic)

  output.clear();
  for (auto chr : utf8::decoder(form.str, form.len)) {
    // Arabic normalization
    if (chr == 0x640 || (chr >= 0x64B && chr <= 0x657) || chr == 0x670) {}
    else if (chr == 0x622) utf8::append(output, 0x627);
    else if (chr == 0x623) utf8::append(output, 0x627);
    else if (chr == 0x624) utf8::append(output, 0x648);
    else if (chr == 0x625) utf8::append(output, 0x627);
    else if (chr == 0x626) utf8::append(output, 0x64A);
    else if (chr == 0x671) utf8::append(output, 0x627);
    else if (chr == 0x6A9) utf8::append(output, 0x643);
    else if (chr == 0x6AA) utf8::append(output, 0x643);
    else if (chr == 0x6CC) utf8::append(output, 0x64A);
    // Space normalization for tagger
    else if (chr == ' ' && also_spaces) utf8::append(output, 0x01);
    // Default
    else utf8::append(output, chr);
  }

  // Make sure we do not remove everything
  if (output.empty() && form.len)
    utf8::append(output, utf8::first(form.str, form.len));

  return output;
}

} // namespace udpipe
} // namespace ufal
