/*!
 * \file main.h
 *
 * \author Sicris Rey Embay
 */
#ifndef MAIN_H_
#define MAIN_H_

#define ASSERT_ENABLE   (1)

#if ASSERT_ENABLE
#define ASSERT_ME(x)        \
    if(!(x)) {                 \
        __disable_irq();    \
        while(1);           \
    }
#else
#define ASSERT_ME(x)
#endif

#endif /* MAIN_H_ */
