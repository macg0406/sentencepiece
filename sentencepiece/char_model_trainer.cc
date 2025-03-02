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

#include "sentencepiece/char_model_trainer.h"

#include <cmath>

#include "sentencepiece/char_model.h"
#include "sentencepiece/util.h"

namespace sentencepiece {
namespace character {

util::Status Trainer::Train() {
  RETURN_IF_ERROR(status());

  SPLOG(INFO) << "Starts training with : \n" << trainer_spec_.Utf8DebugString();

  CHECK_OR_RETURN(normalizer_spec_.escape_whitespaces());
  CHECK_EQ_OR_RETURN(TrainerSpec::CHAR, trainer_spec_.model_type());

  RETURN_IF_ERROR(LoadSentences());

  const int vocab_size = trainer_spec_.vocab_size() - meta_pieces_.size();
  CHECK_GE_OR_RETURN(vocab_size, 0);

  uint64 sum = 0;
  for (const auto &it : required_chars_) {
    sum += it.second;
  }

  const float logsum = log(sum);

  CHECK_OR_RETURN(final_pieces_.empty());
  for (const auto &it : Sorted(required_chars_)) {
    if (final_pieces_.size() == static_cast<size_t>(vocab_size)) {
      break;
    }
    final_pieces_.emplace_back(string_util::UnicodeCharToUTF8(it.first),
                               log(it.second) - logsum);
  }

  return Save();
}
}  // namespace character
}  // namespace sentencepiece
