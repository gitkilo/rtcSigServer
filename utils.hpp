//
//  utils.hpp
//  sign_server
//
//  Created by chifl on 2020/6/25.
//  Copyright © 2020 chifl. All rights reserved.
//

#ifndef utils_hpp
#define utils_hpp

#include <stddef.h>
#include <string>

#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

std::string int2str(int i);
std::string size_t2str(size_t i);

std::string ToString(int s);
std::string ToString(unsigned long s);
#endif /* utils_hpp */
