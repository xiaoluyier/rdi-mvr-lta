#!/usr/bin/env python
#
# Small test suite of Kepler compiler
#
# Author:  Raul E. Monti
# Date:    15.10.2021
# License: GPLv3
#
from kepler import Kepler2IOSA

if __name__ == "__main__":
    
    print("##################################################################")
    print("#                 TESTING ON KEPLER INPUT                        #")
    print("##################################################################\n\n")
    
    kepler_input = '''
        toplevel "and1";
        "and1" and "b1" "or1"; 
        "b1" fail~uniform(0,1) repair~exponential(0.123123e-5); 
        "or1" or "b2" "p1"; 
        "b2" fail~exponential(0.21) repair~uniform(0.5, 3.5); 
        "p1" pand "b3" "b2";
        "b3" fail~exponential(0.1111e-10) repair~uniform(0.5, 3.5e2);
        "b4" fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "r1" rbox prio "b1" "b2" "b3";'''
    
    print (Kepler2IOSA().compile(kepler_input))
    print (Kepler2IOSA().ifun_structural(kepler_input))
  
    kepler_input2 = '''
        toplevel "SPARE";
        "SPARE" spare "B1" "SBE1" "SBE2";
        "B1" fail~exponential(0.1111e-10) repair~uniform(0.5, 3.5e2);
        "SBE1" dorm~exponential(7.2) fail~exponential(7.2) repair~exponential(7.2);
        "SBE2" dorm~exponential(7.2) fail~exponential(7.2) repair~exponential(7.2);
        '''
    print (Kepler2IOSA().compile(kepler_input2))
    print (Kepler2IOSA().ifun_structural(kepler_input2))

        
    print("\n\n##################################################################")
    print("#                 TESTING ON GALILEO INPUT                       #")
    print("##################################################################\n\n")    
    
    galileo_input = '''
        toplevel "failure"; 
        "failure" seq "b1" "b2"; 
        "b1" lambda=0.123e-2; 
        "b2" dorm = 0.25 res = 0.2;''' 

    print (Kepler2IOSA().compile(galileo_input))
    
    print("\n\n##################################################################")
    print("#                 TESTING Struct IF for Kepler                   #")
    print("##################################################################\n\n")
    
    def make_test(name, input, expected):
        print("### TESTING: {} ###".format(name))
        ifun = Kepler2IOSA().ifun_structural(input)

        
        print((Kepler2IOSA().compile(input)))
        if expected == ifun:
            print ("{} PASSED".format(name))
        else:
            print ("{} FAILED".format(name))
            print ("{} expected: {}".format(name, expected))
            print ("{} found: {}".format(name, ifun))
        print("STRUCT:", ifun)
        try:
            mcs = Kepler2IOSA().ifun_mcs(input)
            mcsn = Kepler2IOSA().ifun_mcsn(input)
            print("MCS:", mcs)
            print("MCSN:", mcsn)
        except Exception as e:
            print(e)
    
    make_test(
        "TEST_AND",
        '''
        toplevel "AND1";
        "AND1" and "B1" "B2";
        "B1" fail~exponential(0.1111e-10) repair~uniform(0.5, 3.5e2);
        "B2" fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        ''',
        ('(B1+B2)', 0, 2)
        )

    make_test(
        "TEST_AND_CASCADE",
        '''
        toplevel "AND1";
        "AND1" and "B1" "AND2";
        "AND2" and "B2" "B3";
        "B1" fail~exponential(0.1111e-10) repair~uniform(0.5, 3.5e2);
        "B2" fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "B3" fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        ''',
        ('((2*B1)+(B2+B3))', 0, 4)
        )

    make_test(
        "TEST_VOT",
        '''
        toplevel "VOT1";
        "VOT1" 2of3 "B1" "B2" "AND1";
        "B1" fail~exponential(0.1111e-10) repair~uniform(0.5, 3.5e2);
        "B2" fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "AND1" and "B3" "B4";
        "B3" fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "B4" fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        ''',
        ("",0,0)
        )

    make_test(
        "TEST_SPARE",
        '''
        toplevel "SPARE";
        "SPARE" spare "B1" "SBE1" "SBE2";
        "B1" fail~exponential(0.1111e-10) repair~uniform(0.5, 3.5e2);
        "SBE1" fail~exponential(7.2) dorm~exponential(3)   repair~weibull(1.2,0.3);
        "SBE2" dorm~exponential(3)   fail~exponential(7.2) repair~weibull(1.2,0.3);
        ''',
        ("",0,0)
        )

    make_test(
        "TEST_VOT_MARCO",
        '''toplevel "System";
        "System" pand "VotA" "VotB";
        "VotA" 3of6 "BE1" "BE2" "BE3" "BE4" "BE5" "BE6";
        "VotB" 5of7 "BE4" "BE5" "BE6" "BE7" "BE8" "BE9" "BE10";
        "BE1"  fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "BE2"  fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "BE3"  fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "BE4"  fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "BE5"  fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "BE6"  fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "BE7"  fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "BE8"  fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "BE9"  fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "BE10" fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "RB" rbox prio "BE1" "BE4" "BE2" "BE5" "BE3" "BE6" "BE7" "BE8" "BE9" "BE10";
        ''',
        ("max(0,15.0*PAND_12+5*(summax(3,BE_0,BE_1,BE_2,BE_3,BE_4,BE_5))+(PAND_12>0?3*(summax(5,BE_3,BE_4,BE_5,BE_7,BE_8,BE_9,BE_10)):-3*(summax(5,BE_3,BE_4,BE_5,BE_7,BE_8,BE_9,BE_10)",0,75)
    )

    make_test(
        "TEST_FDEP",
        '''toplevel "AND1";
        "AND1" and "BE1" "BE2";
        "BE1"  fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "BE2"  fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "BE3"  fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "FDEP1" fdep "BE3" "BE1" "BE2";
        ''',
        ("",0,75)
    )

    make_test(
        "TEST_VOT",
        '''toplevel "V1";
        "V1" 2of3 "B1" "B2" "B3";
        "B1" fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "B2" fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "B3" fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "RB" rbox prio "B1" "B2" "B3";
        ''',
        ("",0,0)
    )

    make_test(
        "TEST_PAND",
        '''toplevel "PAND1";
        "PAND1" pand "B1" "B2";
        "B1" fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "B2" fail~exponential(3.11e-10) repair~uniform(5, 3.5e2);
        "RB" rbox prio "B1" "B2";
        ''',
        ("",0,0)
    )

    make_test(
        "TEST DSPARE_3",
        '''
        toplevel "System";
        "System" and "SP1" "SP2" "SP3";
        "SP1" spare "BE1" "SBE1" "SBE2" "SBE3";
        "SP2" spare "BE2" "SBE1" "SBE2" "SBE3";
        "SP3" spare "BE3" "SBE1" "SBE2" "SBE3";
        "BE1"  fail~exponential(0.07) repair~uniform(1,2);
        "BE2"  fail~exponential(0.07) repair~uniform(1,2);
        "BE3"  fail~exponential(0.07) repair~uniform(1,2);
        "SBE1" fail~exponential(0.07) repair~uniform(1,2) dorm~exponential(0.035);
        "SBE2" fail~exponential(0.07) repair~uniform(1,2) dorm~exponential(0.035);
        "SBE3" fail~exponential(0.07) repair~uniform(1,2) dorm~exponential(0.035);
        "RC" rbox prio "SBE1" "SBE2" "SBE3" "BE1" "BE2" "BE3";
        ''',
        ("",0,0)
    )

    make_test(
        "TEST FOO",
        """
        toplevel "TLE";
        "TLE" spare "B1" "B2";
        "B1" fail~exponential(4.5e-4) repair~uniform(.5,.55);
        "B2" fail~exponential(6.8e-4) repair~uniform(.3,.44) dorm~exponential(2.8e-7);
        "RBOX" rbox prio "B2" "B1";
        """
        ,
        ("",0,0)

    )

    make_test(
        "STT Journal tool example",
        """toplevel "Gate3";
        "Gate3" and "BE_E" "BE_F" "BE_G";
        "BE_E" fail~exponential(6.0E-5)
        repair~uniform(8,24);
        "BE_F" fail~exponential(7.0E-5)
        repair~uniform(8,24);
        "BE_G" fail~exponential(6.0E-5)
        repair~uniform(8,12);
        "RB1" rbox prio "BE_E" "BE_F" "BE_G";
        """,
        ("",0,0)
     )

    make_test("DSPARE_3",
        """toplevel "System";
        "System" and "SP1" "SP2" "SP3";
        "SP1" spare "BE1" "SBE1" "SBE2" "SBE3";
        "SP2" spare "BE2" "SBE1" "SBE2" "SBE3";
        "SP3" spare "BE3" "SBE1" "SBE2" "SBE3";
        "BE1"  fail~exponential(0.07) repair~uniform(1,2);
        "BE2"  fail~exponential(0.07) repair~uniform(1,2);
        "BE3"  fail~exponential(0.07) repair~uniform(1,2);
        "SBE1" fail~exponential(0.07) repair~uniform(1,2) dorm~exponential(0.035);
        "SBE2" fail~exponential(0.07) repair~uniform(1,2) dorm~exponential(0.035);
        "SBE3" fail~exponential(0.07) repair~uniform(1,2) dorm~exponential(0.035);
        "RC" rbox prio "SBE1" "SBE2" "SBE3" "BE1" "BE2" "BE3";
        """, 
        ("",0,0)
    )