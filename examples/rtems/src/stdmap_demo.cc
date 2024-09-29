/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2024 Phytium Technology Co., Ltd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * FilePath: stdmap_demo.cc
 * Created Date: 2023-10-27 17:02:35
 * Last Modified: 2023-10-27 09:22:20
 * Description:  This file is for RTEMS C++ STL map demo
 *
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 *  1.0     zhugengyu  2024/08/20    first release
 */
#include <iostream>
#include <map>
#include <set>
#include <algorithm>
#include <functional>

#include "stdmap_demo.h"

int stdmap_demo_cc(void)
{

  /* Creating & Initializing a map of String & Ints */
  std::map<std::string, int> mapOfWordCount = { { "aaa", 10 }, { "ddd", 41 },
  		{ "bbb", 62 }, { "ccc", 13 } };

  /* Declaring the type of Predicate that accepts 2 pairs and return a bool  */
  typedef std::function<bool(std::pair<std::string, int>, std::pair<std::string, int>)> Comparator;

  /* Defining a lambda function to compare two pairs. It will compare two pairs using second field  */
  Comparator compFunctor =
  		[](std::pair<std::string, int> elem1 ,std::pair<std::string, int> elem2)
  		{
  			return elem1.second < elem2.second;
  		};

  /* Declaring a set that will store the pairs using above comparision logic  */
  std::set<std::pair<std::string, int>, Comparator> setOfWords(
  		mapOfWordCount.begin(), mapOfWordCount.end(), compFunctor);

  /* Iterate over a set using range base for loop */
  /* It will display the items in sorted order of values */
  for (std::pair<std::string, int> element : setOfWords)
  	std::cout << element.first << " :: " << element.second << std::endl;

  return 0;
}

extern "C" int stdmap_demo(void)
{
  return stdmap_demo_cc();
}