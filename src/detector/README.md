# Detector module - README

## Description
This NEMEA module is intended for detection.

## RUN
    * First interface is for input UniRec data
    * Second interface is for detected alerts
    * -v specifies verbose mode (you can use three levels)
    ./prg -i "u:zwave,u:alert" -v
## CONFIGURATION FILE STRUCTURE
    <> ... item explenation
    [] ... optional value
    ##################
    ##### Header #####
    ##################
    <UniRec field name>[,<Value ID>]:
    ################
    ##### Body #####
    ################
    ### General Section ###
    <Time series lenght>;
    <Learning lenght>;
    <Ignoring lenght>;
    <Saving mode>;
    [<Periodic check interval>];
    [<Max limit for periodic data check>];
    [<Export interval>];
    ### Profile Section ###
    * Current options: average, moving_average, moving_variance, moving_median
    profile(<Sequence of profile items separated by comma>);
    ### Profile Values Section ###
    * Must be defined for each profile item above
    * Last two zeros are necessary for implementation (stores soft min and max counter to identify grace period exceedances);
    <Profle Name>([<Soft min limit>,<Soft max limit>,<Hard min limit>,<Hard max limit>,<Grace period>,<Min grow limit>,<Max grow limit>],0,0)
    ### Export Section ###
    export([<Sequence of profile items used for periodic export>])
