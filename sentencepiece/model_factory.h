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

#ifndef MODEL_FACTORY_H_
#define MODEL_FACTORY_H_

#include <memory>
#include "sentencepiece/model_interface.h"
#include "sentencepiece/sentencepiece_model.pb.h"

namespace sentencepiece {

class ModelFactory {
 public:
  // Creates Model instance from |model_proto|.
  static std::unique_ptr<ModelInterface> Create(const ModelProto &model_proto);
};
}  // namespace sentencepiece
#endif  // MODEL_FACTORY_H_
