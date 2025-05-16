//
// Created by stefan on 5/16/25.
//

#pragma once

#ifdef CONSTEXPR_TESTING

#define CONSTEXPR constexpr
#define ASSERT STATIC_CHECK

#else

#define CONSTEXPR
#define ASSERT CHECK

#endif