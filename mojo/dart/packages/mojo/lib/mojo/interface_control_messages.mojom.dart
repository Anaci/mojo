// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

library interface_control_messages_mojom;

import 'dart:async';

import 'package:mojo/bindings.dart' as bindings;
import 'package:mojo/core.dart' as core;
const int kRunMessageId = 0xFFFFFFFF;
const int kRunOrClosePipeMessageId = 0xFFFFFFFE;



class RunMessageParams extends bindings.Struct {
  static const List<bindings.StructDataHeader> kVersions = const [
    const bindings.StructDataHeader(24, 0)
  ];
  int reserved0 = 0;
  int reserved1 = 0;
  QueryVersion queryVersion = null;

  RunMessageParams() : super(kVersions.last.size);

  static RunMessageParams deserialize(bindings.Message message) {
    var decoder = new bindings.Decoder(message);
    var result = decode(decoder);
    if (decoder.excessHandles != null) {
      decoder.excessHandles.forEach((h) => h.close());
    }
    return result;
  }

  static RunMessageParams decode(bindings.Decoder decoder0) {
    if (decoder0 == null) {
      return null;
    }
    RunMessageParams result = new RunMessageParams();

    var mainDataHeader = decoder0.decodeStructDataHeader();
    if (mainDataHeader.version <= kVersions.last.version) {
      // Scan in reverse order to optimize for more recent versions.
      for (int i = kVersions.length - 1; i >= 0; --i) {
        if (mainDataHeader.version >= kVersions[i].version) {
          if (mainDataHeader.size == kVersions[i].size) {
            // Found a match.
            break;
          }
          throw new bindings.MojoCodecError(
              'Header size doesn\'t correspond to known version size.');
        }
      }
    } else if (mainDataHeader.size < kVersions.last.size) {
      throw new bindings.MojoCodecError(
        'Message newer than the last known version cannot be shorter than '
        'required by the last known version.');
    }
    if (mainDataHeader.version >= 0) {
      
      result.reserved0 = decoder0.decodeUint32(8);
    }
    if (mainDataHeader.version >= 0) {
      
      result.reserved1 = decoder0.decodeUint32(12);
    }
    if (mainDataHeader.version >= 0) {
      
      var decoder1 = decoder0.decodePointer(16, false);
      result.queryVersion = QueryVersion.decode(decoder1);
    }
    return result;
  }

  void encode(bindings.Encoder encoder) {
    var encoder0 = encoder.getStructEncoderAtOffset(kVersions.last);
    
    encoder0.encodeUint32(reserved0, 8);
    
    encoder0.encodeUint32(reserved1, 12);
    
    encoder0.encodeStruct(queryVersion, 16, false);
  }

  String toString() {
    return "RunMessageParams("
           "reserved0: $reserved0" ", "
           "reserved1: $reserved1" ", "
           "queryVersion: $queryVersion" ")";
  }

  Map toJson() {
    Map map = new Map();
    map["reserved0"] = reserved0;
    map["reserved1"] = reserved1;
    map["queryVersion"] = queryVersion;
    return map;
  }
}


class RunResponseMessageParams extends bindings.Struct {
  static const List<bindings.StructDataHeader> kVersions = const [
    const bindings.StructDataHeader(24, 0)
  ];
  int reserved0 = 0;
  int reserved1 = 0;
  QueryVersionResult queryVersionResult = null;

  RunResponseMessageParams() : super(kVersions.last.size);

  static RunResponseMessageParams deserialize(bindings.Message message) {
    var decoder = new bindings.Decoder(message);
    var result = decode(decoder);
    if (decoder.excessHandles != null) {
      decoder.excessHandles.forEach((h) => h.close());
    }
    return result;
  }

  static RunResponseMessageParams decode(bindings.Decoder decoder0) {
    if (decoder0 == null) {
      return null;
    }
    RunResponseMessageParams result = new RunResponseMessageParams();

    var mainDataHeader = decoder0.decodeStructDataHeader();
    if (mainDataHeader.version <= kVersions.last.version) {
      // Scan in reverse order to optimize for more recent versions.
      for (int i = kVersions.length - 1; i >= 0; --i) {
        if (mainDataHeader.version >= kVersions[i].version) {
          if (mainDataHeader.size == kVersions[i].size) {
            // Found a match.
            break;
          }
          throw new bindings.MojoCodecError(
              'Header size doesn\'t correspond to known version size.');
        }
      }
    } else if (mainDataHeader.size < kVersions.last.size) {
      throw new bindings.MojoCodecError(
        'Message newer than the last known version cannot be shorter than '
        'required by the last known version.');
    }
    if (mainDataHeader.version >= 0) {
      
      result.reserved0 = decoder0.decodeUint32(8);
    }
    if (mainDataHeader.version >= 0) {
      
      result.reserved1 = decoder0.decodeUint32(12);
    }
    if (mainDataHeader.version >= 0) {
      
      var decoder1 = decoder0.decodePointer(16, false);
      result.queryVersionResult = QueryVersionResult.decode(decoder1);
    }
    return result;
  }

  void encode(bindings.Encoder encoder) {
    var encoder0 = encoder.getStructEncoderAtOffset(kVersions.last);
    
    encoder0.encodeUint32(reserved0, 8);
    
    encoder0.encodeUint32(reserved1, 12);
    
    encoder0.encodeStruct(queryVersionResult, 16, false);
  }

  String toString() {
    return "RunResponseMessageParams("
           "reserved0: $reserved0" ", "
           "reserved1: $reserved1" ", "
           "queryVersionResult: $queryVersionResult" ")";
  }

  Map toJson() {
    Map map = new Map();
    map["reserved0"] = reserved0;
    map["reserved1"] = reserved1;
    map["queryVersionResult"] = queryVersionResult;
    return map;
  }
}


class QueryVersion extends bindings.Struct {
  static const List<bindings.StructDataHeader> kVersions = const [
    const bindings.StructDataHeader(8, 0)
  ];

  QueryVersion() : super(kVersions.last.size);

  static QueryVersion deserialize(bindings.Message message) {
    var decoder = new bindings.Decoder(message);
    var result = decode(decoder);
    if (decoder.excessHandles != null) {
      decoder.excessHandles.forEach((h) => h.close());
    }
    return result;
  }

  static QueryVersion decode(bindings.Decoder decoder0) {
    if (decoder0 == null) {
      return null;
    }
    QueryVersion result = new QueryVersion();

    var mainDataHeader = decoder0.decodeStructDataHeader();
    if (mainDataHeader.version <= kVersions.last.version) {
      // Scan in reverse order to optimize for more recent versions.
      for (int i = kVersions.length - 1; i >= 0; --i) {
        if (mainDataHeader.version >= kVersions[i].version) {
          if (mainDataHeader.size == kVersions[i].size) {
            // Found a match.
            break;
          }
          throw new bindings.MojoCodecError(
              'Header size doesn\'t correspond to known version size.');
        }
      }
    } else if (mainDataHeader.size < kVersions.last.size) {
      throw new bindings.MojoCodecError(
        'Message newer than the last known version cannot be shorter than '
        'required by the last known version.');
    }
    return result;
  }

  void encode(bindings.Encoder encoder) {
    encoder.getStructEncoderAtOffset(kVersions.last);
  }

  String toString() {
    return "QueryVersion("")";
  }

  Map toJson() {
    Map map = new Map();
    return map;
  }
}


class QueryVersionResult extends bindings.Struct {
  static const List<bindings.StructDataHeader> kVersions = const [
    const bindings.StructDataHeader(16, 0)
  ];
  int version = 0;

  QueryVersionResult() : super(kVersions.last.size);

  static QueryVersionResult deserialize(bindings.Message message) {
    var decoder = new bindings.Decoder(message);
    var result = decode(decoder);
    if (decoder.excessHandles != null) {
      decoder.excessHandles.forEach((h) => h.close());
    }
    return result;
  }

  static QueryVersionResult decode(bindings.Decoder decoder0) {
    if (decoder0 == null) {
      return null;
    }
    QueryVersionResult result = new QueryVersionResult();

    var mainDataHeader = decoder0.decodeStructDataHeader();
    if (mainDataHeader.version <= kVersions.last.version) {
      // Scan in reverse order to optimize for more recent versions.
      for (int i = kVersions.length - 1; i >= 0; --i) {
        if (mainDataHeader.version >= kVersions[i].version) {
          if (mainDataHeader.size == kVersions[i].size) {
            // Found a match.
            break;
          }
          throw new bindings.MojoCodecError(
              'Header size doesn\'t correspond to known version size.');
        }
      }
    } else if (mainDataHeader.size < kVersions.last.size) {
      throw new bindings.MojoCodecError(
        'Message newer than the last known version cannot be shorter than '
        'required by the last known version.');
    }
    if (mainDataHeader.version >= 0) {
      
      result.version = decoder0.decodeUint32(8);
    }
    return result;
  }

  void encode(bindings.Encoder encoder) {
    var encoder0 = encoder.getStructEncoderAtOffset(kVersions.last);
    
    encoder0.encodeUint32(version, 8);
  }

  String toString() {
    return "QueryVersionResult("
           "version: $version" ")";
  }

  Map toJson() {
    Map map = new Map();
    map["version"] = version;
    return map;
  }
}


class RunOrClosePipeMessageParams extends bindings.Struct {
  static const List<bindings.StructDataHeader> kVersions = const [
    const bindings.StructDataHeader(24, 0)
  ];
  int reserved0 = 0;
  int reserved1 = 0;
  RequireVersion requireVersion = null;

  RunOrClosePipeMessageParams() : super(kVersions.last.size);

  static RunOrClosePipeMessageParams deserialize(bindings.Message message) {
    var decoder = new bindings.Decoder(message);
    var result = decode(decoder);
    if (decoder.excessHandles != null) {
      decoder.excessHandles.forEach((h) => h.close());
    }
    return result;
  }

  static RunOrClosePipeMessageParams decode(bindings.Decoder decoder0) {
    if (decoder0 == null) {
      return null;
    }
    RunOrClosePipeMessageParams result = new RunOrClosePipeMessageParams();

    var mainDataHeader = decoder0.decodeStructDataHeader();
    if (mainDataHeader.version <= kVersions.last.version) {
      // Scan in reverse order to optimize for more recent versions.
      for (int i = kVersions.length - 1; i >= 0; --i) {
        if (mainDataHeader.version >= kVersions[i].version) {
          if (mainDataHeader.size == kVersions[i].size) {
            // Found a match.
            break;
          }
          throw new bindings.MojoCodecError(
              'Header size doesn\'t correspond to known version size.');
        }
      }
    } else if (mainDataHeader.size < kVersions.last.size) {
      throw new bindings.MojoCodecError(
        'Message newer than the last known version cannot be shorter than '
        'required by the last known version.');
    }
    if (mainDataHeader.version >= 0) {
      
      result.reserved0 = decoder0.decodeUint32(8);
    }
    if (mainDataHeader.version >= 0) {
      
      result.reserved1 = decoder0.decodeUint32(12);
    }
    if (mainDataHeader.version >= 0) {
      
      var decoder1 = decoder0.decodePointer(16, false);
      result.requireVersion = RequireVersion.decode(decoder1);
    }
    return result;
  }

  void encode(bindings.Encoder encoder) {
    var encoder0 = encoder.getStructEncoderAtOffset(kVersions.last);
    
    encoder0.encodeUint32(reserved0, 8);
    
    encoder0.encodeUint32(reserved1, 12);
    
    encoder0.encodeStruct(requireVersion, 16, false);
  }

  String toString() {
    return "RunOrClosePipeMessageParams("
           "reserved0: $reserved0" ", "
           "reserved1: $reserved1" ", "
           "requireVersion: $requireVersion" ")";
  }

  Map toJson() {
    Map map = new Map();
    map["reserved0"] = reserved0;
    map["reserved1"] = reserved1;
    map["requireVersion"] = requireVersion;
    return map;
  }
}


class RequireVersion extends bindings.Struct {
  static const List<bindings.StructDataHeader> kVersions = const [
    const bindings.StructDataHeader(16, 0)
  ];
  int version = 0;

  RequireVersion() : super(kVersions.last.size);

  static RequireVersion deserialize(bindings.Message message) {
    var decoder = new bindings.Decoder(message);
    var result = decode(decoder);
    if (decoder.excessHandles != null) {
      decoder.excessHandles.forEach((h) => h.close());
    }
    return result;
  }

  static RequireVersion decode(bindings.Decoder decoder0) {
    if (decoder0 == null) {
      return null;
    }
    RequireVersion result = new RequireVersion();

    var mainDataHeader = decoder0.decodeStructDataHeader();
    if (mainDataHeader.version <= kVersions.last.version) {
      // Scan in reverse order to optimize for more recent versions.
      for (int i = kVersions.length - 1; i >= 0; --i) {
        if (mainDataHeader.version >= kVersions[i].version) {
          if (mainDataHeader.size == kVersions[i].size) {
            // Found a match.
            break;
          }
          throw new bindings.MojoCodecError(
              'Header size doesn\'t correspond to known version size.');
        }
      }
    } else if (mainDataHeader.size < kVersions.last.size) {
      throw new bindings.MojoCodecError(
        'Message newer than the last known version cannot be shorter than '
        'required by the last known version.');
    }
    if (mainDataHeader.version >= 0) {
      
      result.version = decoder0.decodeUint32(8);
    }
    return result;
  }

  void encode(bindings.Encoder encoder) {
    var encoder0 = encoder.getStructEncoderAtOffset(kVersions.last);
    
    encoder0.encodeUint32(version, 8);
  }

  String toString() {
    return "RequireVersion("
           "version: $version" ")";
  }

  Map toJson() {
    Map map = new Map();
    map["version"] = version;
    return map;
  }
}


