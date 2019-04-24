import ctypes
import ctypes.util
from .util import CwipcError, cwipc, cwipc_source, cwipc_point, cwipc_point_array
from .util import cwipc_p, cwipc_source_p

__all__ = [
    "cwpic_decompress"
]
    
class cwipc_encoder_p(ctypes.c_void_p):
    pass
    
class cwipc_decoder_p(cwipc_source_p):
    pass
    
_cwipc_codec_dll_reference = None

#
# NOTE: the signatures here must match those in cwipc_util/api.h or all hell will break loose
#
def _cwipc_codec_dll(libname=None):
    """Load the cwipc_util DLL and assign the signatures (if not already loaded)"""
    global _cwipc_codec_dll_reference
    if _cwipc_codec_dll_reference: return _cwipc_codec_dll_reference
    
    if libname == None:
        libname = ctypes.util.find_library('cwipc_codec')
        if not libname:
            raise RuntimeError('Dynamic library cwipc_util not found')
    assert libname
    _cwipc_codec_dll_reference = ctypes.CDLL(libname)
    

    _cwipc_codec_dll_reference.cwipc_new_encoder.argtypes = [ctypes.c_int, ctypes.POINTER(cwipc_encoder_params)]
    _cwipc_codec_dll_reference.cwipc_new_encoder.restype = cwipc_encoder_p
    _cwipc_codec_dll_reference.cwipc_encoder_free.argtypes = [cwipc_encoder_p]
    _cwipc_codec_dll_reference.cwipc_encoder_free.restype = None
    _cwipc_codec_dll_reference.cwipc_encoder_available.argtypes = [cwipc_encoder_p, ctypes.c_bool]
    _cwipc_codec_dll_reference.cwipc_encoder_available.restype = ctypes.c_bool
    _cwipc_codec_dll_reference.cwipc_encoder_eof.argtypes = [cwipc_encoder_p]
    _cwipc_codec_dll_reference.cwipc_encoder_eof.restype = ctypes.c_bool
    _cwipc_codec_dll_reference.cwipc_encoder_at_gop_boundary.argtypes = [cwipc_encoder_p]
    _cwipc_codec_dll_reference.cwipc_encoder_at_gop_boundary.restype = ctypes.c_bool
    _cwipc_codec_dll_reference.cwipc_encoder_feed.argtypes = [cwipc_encoder_p, cwipc_p]
    _cwipc_codec_dll_reference.cwipc_encoder_feed.restype = None
    _cwipc_codec_dll_reference.cwipc_encoder_get_encoded_size.argtypes = [cwipc_encoder_p]
    _cwipc_codec_dll_reference.cwipc_encoder_get_encoded_size.restype = ctypes.c_size_t
    _cwipc_codec_dll_reference.cwipc_encoder_copy_data.argtypes = [cwipc_encoder_p, ctypes.c_void_p, ctypes.c_size_t]
    _cwipc_codec_dll_reference.cwipc_encoder_copy_data.restype = ctypes.c_bool

    _cwipc_codec_dll_reference.cwipc_new_decoder.argtypes = []
    _cwipc_codec_dll_reference.cwipc_new_decoder.restype = cwipc_decoder_p
    _cwipc_codec_dll_reference.cwipc_decoder_feed.argtypes = [cwipc_decoder_p, ctypes.c_void_p, ctypes.c_size_t]
    _cwipc_codec_dll_reference.cwipc_decoder_feed.restype = None


    return _cwipc_codec_dll_reference

class cwipc_encoder_params(ctypes.Structure):
    """Parameters to control cwipc compression"""
    _fields_ = [
        ("num_threads", ctypes.c_int),
        ("do_inter_frame", ctypes.c_bool),
        ("gop_size", ctypes.c_int),
        ("exp_factor", ctypes.c_double),
        ("octree_bits", ctypes.c_int),
        ("color_bits", ctypes.c_int),
        ("jpeg_quality", ctypes.c_int),
        ("macroblock_size", ctypes.c_int)        
    ]
CWIPC_ENCODER_PARAM_VERSION = 0x20190330

class cwipc_encoder_wrapper:
    def __init__(self, _cwipc_encoder):
        if _cwipc_encoder != None:
            assert isinstance(_cwipc_encoder, cwipc_encoder_p)
        self._cwipc_encoder = _cwipc_encoder
        
    def _as_cwipc_encoder_p(self):
        assert self._cwipc_encoder
        return self._cwipc_encoder
        
    def free(self):
        if self._cwipc_encoder:
            _cwipc_codec_dll().cwipc_encoder_free(self._as_cwipc_encoder_p())
        self._cwipc_encoder = None
        
    def eof(self):
        rv = _cwipc_codec_dll().cwipc_encoder_eof(self._as_cwipc_encoder_p())
        return rv
        
    def at_gop_boundary(self):
        rv = _cwipc_codec_dll().cwipc_encoder_at_gop_boundary(self._as_cwipc_encoder_p())
        return rv
        
    def available(self, wait):
        rv = _cwipc_codec_dll().cwipc_encoder_available(self._as_cwipc_encoder_p(), wait)
        return rv
        
    def feed(self, pc):
        rv = _cwipc_codec_dll().cwipc_encoder_feed(self._as_cwipc_encoder_p(), pc._as_cwipc_p())
        return rv
        
    def get_encoded_size(self):
        rv = _cwipc_codec_dll().cwipc_encoder_get_encoded_size(self._as_cwipc_encoder_p())
        return rv
        
    def get_bytes(self):
        length = self.get_encoded_size()
        rv = bytearray(length)
        ptr_char = (ctypes.c_char * length).from_buffer(rv)
        ptr = ctypes.cast(ptr_char, ctypes.c_void_p)
        ok = _cwipc_codec_dll().cwipc_encoder_copy_data(self._as_cwipc_encoder_p(), ptr, length)
        if not ok:
            return None
        return rv
    
class cwipc_decoder_wrapper(cwipc_source):
    def __init__(self, _cwipc_decoder):
        if _cwipc_decoder != None:
            assert isinstance(_cwipc_decoder, cwipc_decoder_p)
        cwipc_source.__init__(self, _cwipc_decoder)
        
    def _as_cwipc_decoder_p(self):
        assert self._cwipc_source
        return self._cwipc_source
        
    def feed(self, buffer):
        length = len(buffer)
        if isinstance(buffer, bytearray):
            buffer = (ctypes.c_char * length).from_buffer(buffer)
        ptr = ctypes.cast(buffer, ctypes.c_void_p)
        rv = _cwipc_codec_dll().cwipc_decoder_feed(self._as_cwipc_decoder_p(), ptr, length)
        return rv
    
def cwipc_new_encoder(version=None, params=None, **kwargs):
    if version == None:
        version = CWIPC_ENCODER_PARAM_VERSION
    if isinstance(params, cwipc_encoder_params):
        pass
    else:
        params = cwipc_encoder_params(1, False, 1, 0, 7, 8, 85, 16, **kwargs)
    obj = _cwipc_codec_dll().cwipc_new_encoder(version, params)
    if not obj:
        return None
    return cwipc_encoder_wrapper(obj)
    
def cwipc_new_decoder():
    obj = _cwipc_codec_dll().cwipc_new_decoder()
    if not obj:
        return None
    return cwipc_decoder_wrapper(obj)
    