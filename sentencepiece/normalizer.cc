// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.!

#include "sentencepiece/normalizer.h"

#include <utility>
#include <vector>
#include "absl/strings/string_view.h"
#include "sentencepiece/common.h"
#include "sentencepiece/util.h"
#include "third_party/darts_clone/darts.h"

namespace sentencepiece {
namespace normalizer {

constexpr int Normalizer::kMaxTrieResultsSize;

Normalizer::Normalizer(const NormalizerSpec &spec)
    : spec_(&spec), status_(util::OkStatus()) {
  absl::string_view index = spec.precompiled_charsmap();
  if (index.empty()) {
    SPLOG(INFO) << "precompiled_charsmap is empty. use identity normalization.";
  } else {
    absl::string_view trie_blob, normalized;
    status_ = DecodePrecompiledCharsMap(index, &trie_blob, &normalized);
    if (!status_.ok()) return;

    // Reads the body of double array.
    trie_ = port::MakeUnique<Darts::DoubleArray>();

    // The second arg of set_array is not the size of blob,
    // but the number of double array units.
    trie_->set_array(const_cast<char *>(trie_blob.data()),
                     trie_blob.size() / trie_->unit_size());

    normalized_ = normalized.data();
  }
}

Normalizer::~Normalizer() {}

util::Status Normalizer::Normalize(absl::string_view input,
                                   std::string *normalized,
                                   std::vector<size_t> *norm_to_orig) const {
  norm_to_orig->clear();
  normalized->clear();

  if (input.empty()) {
    return util::OkStatus();
  }

  RETURN_IF_ERROR(status());

  int consumed = 0;

  // Ignores heading space.
  if (spec_->remove_extra_whitespaces()) {
    while (!input.empty()) {
      const auto p = NormalizePrefix(input);
      if (p.first != " ") {
        break;
      }
      input.remove_prefix(p.second);
      consumed += p.second;
    }
  }

  // all chars are whitespace.
  if (input.empty()) {
    return util::OkStatus();
  }

  // Reserves the output buffer to avoid re-allocations.
  const size_t kReservedSize = input.size() * 3;
  normalized->reserve(kReservedSize);
  norm_to_orig->reserve(kReservedSize);

  // Replaces white space with U+2581 (LOWER ONE EIGHT BLOCK)
  // if escape_whitespaces() is set (default = true).
  const absl::string_view kSpaceSymbol = "\xe2\x96\x81";

  // Adds a space symbol as a prefix (default is true)
  // With this prefix, "world" and "hello world" are converted into
  // "_world" and "_hello_world", which help the trainer to extract
  // "_world" as one symbol.
  if (spec_->add_dummy_prefix()) {
    if (spec_->escape_whitespaces()) {
      normalized->append(kSpaceSymbol.data(), kSpaceSymbol.size());
      for (size_t n = 0; n < kSpaceSymbol.size(); ++n) {
        norm_to_orig->push_back(consumed);
      }
    } else {
      normalized->append(" ");
      norm_to_orig->push_back(consumed);
    }
  }

  bool is_prev_space = spec_->remove_extra_whitespaces();
  while (!input.empty()) {
    auto p = NormalizePrefix(input);
    absl::string_view sp = p.first;

    // Removes heading spaces in sentence piece,
    // if the previous sentence piece ends with whitespace.
    while (is_prev_space && string_util::ConsumePrefix(&sp, " ")) {
    }

    if (!sp.empty()) {
      const char *data = sp.data();
      for (size_t n = 0; n < sp.size(); ++n) {
        if (spec_->escape_whitespaces() && data[n] == ' ') {
          // replace ' ' with kSpaceSymbol.
          normalized->append(kSpaceSymbol.data(), kSpaceSymbol.size());
          for (size_t m = 0; m < kSpaceSymbol.size(); ++m) {
            norm_to_orig->push_back(consumed);
          }
        } else {
          *normalized += data[n];
          norm_to_orig->push_back(consumed);
        }
      }
      // Checks whether the last character of sp is whitespace.
      is_prev_space = string_util::EndsWith(sp, " ");
    }

    consumed += p.second;
    input.remove_prefix(p.second);
    if (!spec_->remove_extra_whitespaces()) {
      is_prev_space = false;
    }
  }

  // Ignores tailing space.
  if (spec_->remove_extra_whitespaces()) {
    const absl::string_view space =
        spec_->escape_whitespaces() ? kSpaceSymbol : " ";
    while (string_util::EndsWith(*normalized, space)) {
      const int length = normalized->size() - space.size();
      CHECK_GE_OR_RETURN(length, 0);
      consumed = (*norm_to_orig)[length];
      normalized->resize(length);
      norm_to_orig->resize(length);
    }
  }

  norm_to_orig->push_back(consumed);

  CHECK_EQ_OR_RETURN(norm_to_orig->size(), normalized->size() + 1);

  return util::OkStatus();
}

std::string Normalizer::Normalize(absl::string_view input) const {
  std::vector<size_t> norm_to_orig;
  std::string normalized;
  Normalize(input, &normalized, &norm_to_orig);
  return normalized;
}

std::pair<absl::string_view, int> Normalizer::NormalizePrefix(
    absl::string_view input) const {
  std::pair<absl::string_view, int> result;

  if (input.empty()) return result;

  size_t longest_length = 0;
  int longest_value = 0;

  if (trie_ != nullptr) {
    // Allocates trie_results in stack, which makes the encoding speed 36%
    // faster. (38k sentences/sec => 60k sentences/sec). Builder checks that the
    // result size never exceeds kMaxTrieResultsSize. This array consumes
    // 0.5kByte in stack, which is less than default stack frames (16kByte).
    Darts::DoubleArray::result_pair_type
        trie_results[Normalizer::kMaxTrieResultsSize];

    const size_t num_nodes = trie_->commonPrefixSearch(
        input.data(), trie_results, Normalizer::kMaxTrieResultsSize,
        input.size());

    // Finds the longest rule.
    for (size_t k = 0; k < num_nodes; ++k) {
      if (longest_length == 0 || trie_results[k].length > longest_length) {
        longest_length = trie_results[k].length;  // length of prefix
        longest_value = trie_results[k].value;    // pointer to |normalized_|.
      }
    }
  }

  if (longest_length == 0) {
    size_t length = 0;
    if (!string_util::IsValidDecodeUTF8(input, &length)) {
      // Found a malformed utf8.
      // The rune is set to be 0xFFFD (REPLACEMENT CHARACTER),
      // which is a valid Unicode of three bytes in utf8,
      // but here we only consume one byte.
      result.second = 1;
      static const char kReplacementChar[] = "\xEF\xBF\xBD";
      result.first = absl::string_view(kReplacementChar);
    } else {
      result.second = length;
      result.first = absl::string_view(input.data(), result.second);
    }
  } else {
    result.second = longest_length;
    // No need to pass the size of normalized sentence,
    // since |normalized| is delimitered by "\0".
    result.first = absl::string_view(&normalized_[longest_value]);
  }

  return result;
}

// static
std::string Normalizer::EncodePrecompiledCharsMap(
    absl::string_view trie_blob, absl::string_view normalized) {
  // <trie size(4byte)><double array trie><normalized string>
  std::string blob;
  blob.append(string_util::EncodePOD<uint32>(trie_blob.size()));
  blob.append(trie_blob.data(), trie_blob.size());
  blob.append(normalized.data(), normalized.size());
  return blob;
}

// static
util::Status Normalizer::DecodePrecompiledCharsMap(
    absl::string_view blob, absl::string_view *trie_blob,
    absl::string_view *normalized) {
  uint32 trie_blob_size = 0;
  if (blob.size() <= sizeof(trie_blob_size) ||
      !string_util::DecodePOD<uint32>(
          absl::string_view(blob.data(), sizeof(trie_blob_size)),
          &trie_blob_size) ||
      trie_blob_size >= blob.size()) {
    return util::InternalError("Blob for normalization rule is broken.");
  }

  blob.remove_prefix(sizeof(trie_blob_size));
  *trie_blob = absl::string_view(blob.data(), trie_blob_size);

  blob.remove_prefix(trie_blob_size);
  *normalized = absl::string_view(blob.data(), blob.size());

  return util::OkStatus();
}
}  // namespace normalizer
}  // namespace sentencepiece
