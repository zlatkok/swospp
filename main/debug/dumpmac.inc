%define set_flags(x) dw x

%macro declareStr 1.nolist
%ifstr %1
    [section .strdata]
    %%str:
        db %1, 0
    __SECT__
    dd %%str
%else
    %error "Expecting string parameter"
%endif
%endmacro
