/******************************************************************************
 * Copyright 2020 ETC Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************
 * This file is a part of RDMnet. For more information, go to:
 * https://github.com/ETCLabs/RDMnet
 *****************************************************************************/

#include "load_test_data.h"
#include <cassert>
#include <ios>
#include <string>

namespace rdmnet
{
namespace testing
{
std::vector<uint8_t> LoadTestData(std::ifstream& file)
{
  assert(file.is_open());

  std::vector<uint8_t> data;
  file >> std::hex;

  int byte;
  do
  {
    if (file >> byte)
      data.push_back(static_cast<uint8_t>(byte));
    else
    {
      file.clear();
      std::string remaining_line;
      std::getline(file, remaining_line);
    }
  } while (!file.eof());

  return data;
}

};  // namespace testing
};  // namespace rdmnet
