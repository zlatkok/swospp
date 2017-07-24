; change this macro when version changes

%define ver         1.9
%ifdef OFFLINE_VERSION
    %define verStr      "1.9.2.2 BETA SENSIBLE DAYS"
    %define verStrLen   26
%else
    %define verStr      "1.9.2.2 BETA"
    %define verStrLen   12
%endif
