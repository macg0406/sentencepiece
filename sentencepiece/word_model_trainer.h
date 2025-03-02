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

#ifndef WORD_MODEL_TRAINER_H_
#define WORD_MODEL_TRAINER_H_

#include "sentencepiece/sentencepiece_model.pb.h"
#include "sentencepiece/trainer_interface.h"

namespace sentencepiece {
namespace word {

// Trainer class for word model.
//
// Word model simply counts the frequency of
// space-delimited tokens, then keep top
// |vocab_size| frequent tokens.
class Trainer : public TrainerInterface {
 public:
  Trainer(const TrainerSpec &trainer_spec,
          const NormalizerSpec &normalizer_spec)
      : TrainerInterface::TrainerInterface(trainer_spec, normalizer_spec) {}

  util::Status Train() override;
};
}  // namespace word
}  // namespace sentencepiece
#endif  // WORD_MODEL_TRAINER_H_
