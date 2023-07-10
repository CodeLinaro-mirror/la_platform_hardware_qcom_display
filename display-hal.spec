Summary: display Library
Name: display-hal
Version: 1.0
Release: r0
License: BSD-3-Clause
URL: https://git.codelinaro.org/
Source0: %{name}-%{version}.tar.gz

BuildRequires:	autoconf automake display-commonsys-intf-linux libgbm-dev display-kernel-headers  
BuildRequires:  pkgconfig(libdrm) = 2.4.110

%global debug_package %{nil}

%description
Provide display HAL (Hardware Abstraction Layer) \
libraries. These libraries serves as an abstraction layer between \
physical hardware and software. They provide display driver interfaces, \
allowing program to communicate with the hardware.

%package dev
Summary : header files

%description dev
header files 

%prep
%autosetup -n %{name}

%build
autoreconf -if
%configure CPPFLAGS="-lbsd -I/usr/include/glib-2.0 -I/usr/include/glib-2.0/include -I/usr/include/glib-2.0/glib -I/usr/lib64/glib-2.0/include/"
%configure CFLAGS="-lbsd -I/usr/include/glib-2.0 -I/usr/include/glib-2.0/include -I/usr/include/glib-2.0/glib -I/usr/lib64/glib-2.0/include/"
%make_build

%install
%make_install

mkdir -p %{buildroot}%{_includedir}
cp include/display_color_processing.h %{buildroot}%{_includedir}
cp include/display_properties.h  %{buildroot}%{_includedir}
#cp libdebug/debug_handler.h  %{buildroot}%{_includedir}
cp libdrmutils/drm_interface.h %{buildroot}%{_includedir}
cp libdrmutils/drm_res_mgr.h %{buildroot}%{_includedir}
cp libdrmutils/drm_master.h %{buildroot}%{_includedir}
cp libdrmutils/drm_logger.h %{buildroot}%{_includedir}
cp libdrmutils/drm_lib_loader.h %{buildroot}%{_includedir}

mkdir -p %{buildroot}%{_includedir}/libdebug
cp libdebug/debug_handler.h  %{buildroot}%{_includedir}/libdebug

mkdir -p %{buildroot}%{_includedir}/utils
cp sdm/include/utils/constants.h %{buildroot}%{_includedir}/utils
cp sdm/include/utils/debug.h %{buildroot}%{_includedir}/utils
cp sdm/include/utils/factory.h %{buildroot}%{_includedir}/utils
cp sdm/include/utils/formats.h %{buildroot}%{_includedir}/utils
cp sdm/include/utils/locker.h %{buildroot}%{_includedir}/utils
cp sdm/include/utils/rect.h %{buildroot}%{_includedir}/utils
cp sdm/include/utils/sync_task.h %{buildroot}%{_includedir}/utils
cp sdm/include/utils/sys.h %{buildroot}%{_includedir}/utils
cp sdm/include/utils/utils.h %{buildroot}%{_includedir}/utils

mkdir -p %{buildroot}%{_includedir}/core
cp sdm/include/core/buffer_allocator.h %{buildroot}%{_includedir}/core
cp sdm/include/core/buffer_sync_handler.h %{buildroot}%{_includedir}/core
cp sdm/include/core/core_interface.h %{buildroot}%{_includedir}/core
cp sdm/include/core/display_interface.h %{buildroot}%{_includedir}/core
cp sdm/include/core/dpps_interface.h %{buildroot}%{_includedir}/core
cp sdm/include/core/layer_buffer.h %{buildroot}%{_includedir}/core
cp sdm/include/core/layer_stack.h %{buildroot}%{_includedir}/core
cp sdm/include/core/notifier_interface.h %{buildroot}%{_includedir}/core
cp sdm/include/core/sdm_types.h %{buildroot}%{_includedir}/core
cp sdm/include/core/socket_handler.h %{buildroot}%{_includedir}/core

mkdir -p %{buildroot}%{_includedir}/private
cp sdm/include/private/color_interface.h %{buildroot}%{_includedir}/private
cp sdm/include/private/color_params.h %{buildroot}%{_includedir}/private
cp sdm/include/private/dpps_control_interface.h %{buildroot}%{_includedir}/private
cp sdm/include/private/extension_interface.h %{buildroot}%{_includedir}/private
cp sdm/include/private/hw_info_interface.h %{buildroot}%{_includedir}/private
cp sdm/include/private/hw_info_types.h %{buildroot}%{_includedir}/private
cp sdm/include/private/partial_update_interface.h %{buildroot}%{_includedir}/private
cp sdm/include/private/resource_interface.h %{buildroot}%{_includedir}/private
cp sdm/include/private/strategy_interface.h %{buildroot}%{_includedir}/private

%files
%{_usr}/lib64/libdisplaydebug.la
%{_usr}/lib64/libdisplaydebug.so
%{_usr}/lib64/libdrmutils.la
%{_usr}/lib64/libdrmutils.so
%{_usr}/lib64/libsdmcore.la
%{_usr}/lib64/libsdmcore.so
%{_usr}/lib64/libsdmutils.la
%{_usr}/lib64/libsdmutils.so

%files dev
%{_includedir}/display_color_processing.h
%{_includedir}/display_properties.h
%{_includedir}/libdebug/debug_handler.h
%{_includedir}/debug_handler.h
%{_includedir}/drm_interface.h
%{_includedir}/drm_res_mgr.h
%{_includedir}/drm_master.h
%{_includedir}/drm_logger.h
%{_includedir}/drm_lib_loader.h

%{_includedir}/utils/constants.h
%{_includedir}/utils/debug.h
%{_includedir}/utils/factory.h
%{_includedir}/utils/formats.h
%{_includedir}/utils/locker.h
%{_includedir}/utils/rect.h
%{_includedir}/utils/sync_task.h
%{_includedir}/utils/sys.h
%{_includedir}/utils/utils.h

%{_includedir}/core/buffer_allocator.h
%{_includedir}/core/buffer_sync_handler.h
%{_includedir}/core/core_interface.h
%{_includedir}/core/display_interface.h
%{_includedir}/core/dpps_interface.h
%{_includedir}/core/layer_buffer.h
%{_includedir}/core/layer_stack.h
%{_includedir}/core/notifier_interface.h
%{_includedir}/core/sdm_types.h
%{_includedir}/core/socket_handler.h



/usr/include/private/color_interface.h
/usr/include/private/color_params.h
/usr/include/private/dpps_control_interface.h
/usr/include/private/extension_interface.h
/usr/include/private/hw_info_interface.h
/usr/include/private/hw_info_types.h
/usr/include/private/partial_update_interface.h
/usr/include/private/resource_interface.h
/usr/include/private/strategy_interface.h
/usr/include/sdm/core/buffer_allocator.h
/usr/include/sdm/core/buffer_sync_handler.h
/usr/include/sdm/core/core_interface.h
/usr/include/sdm/core/display_interface.h
/usr/include/sdm/core/dpps_interface.h
/usr/include/sdm/core/layer_buffer.h
/usr/include/sdm/core/layer_stack.h
/usr/include/sdm/core/notifier_interface.h
/usr/include/sdm/core/sdm_types.h
/usr/include/sdm/core/socket_handler.h
/usr/include/sdm/private/color_interface.h
/usr/include/sdm/private/color_params.h
/usr/include/sdm/private/dpps_control_interface.h
/usr/include/sdm/private/extension_interface.h
/usr/include/sdm/private/hw_info_interface.h
/usr/include/sdm/private/hw_info_types.h
/usr/include/sdm/private/partial_update_interface.h
/usr/include/sdm/private/resource_interface.h
/usr/include/sdm/private/strategy_interface.h
/usr/include/sdm/utils/constants.h
/usr/include/sdm/utils/debug.h
/usr/include/sdm/utils/factory.h
/usr/include/sdm/utils/formats.h
/usr/include/sdm/utils/locker.h
/usr/include/sdm/utils/rect.h
/usr/include/sdm/utils/sync_task.h
/usr/include/sdm/utils/sys.h
/usr/include/sdm/utils/utils.h
