//! RFC 8785 encoding for the compatibility contract's integer-only schema.
//! Property ordering is UTF-16, never UTF-8/code-point order. Reject floating
//! point and unsafe integers instead of producing a platform-dependent ID.
use serde::Serialize;
use serde_json::Value;

pub fn to_vec<T:Serialize>(value:&T)->Result<Vec<u8>,serde_json::Error>{
 let value=serde_json::to_value(value)?;let mut output=Vec::new();encode(&value,&mut output)?;Ok(output)
}
fn invalid()->serde_json::Error{<serde_json::Error as serde::ser::Error>::custom("compatibility JSON numbers must be integers in the interoperable range")}
fn encode(value:&Value,output:&mut Vec<u8>)->Result<(),serde_json::Error>{
 match value {
  Value::Null=>output.extend_from_slice(b"null"),
  Value::Bool(v)=>output.extend_from_slice(if *v{b"true"}else{b"false"}),
  Value::String(v)=>output.extend_from_slice(&serde_json::to_vec(v)?),
  Value::Number(v)=>{let n=v.as_i64().ok_or_else(invalid)?;if !(-9007199254740991..=9007199254740991).contains(&n){return Err(invalid())};output.extend_from_slice(n.to_string().as_bytes());},
  Value::Array(v)=>{output.push(b'[');for (index,item) in v.iter().enumerate(){if index>0{output.push(b',')};encode(item,output)?};output.push(b']');},
  Value::Object(v)=>{let mut entries:Vec<_>=v.iter().collect();entries.sort_by(|(a,_),(b,_)|a.encode_utf16().cmp(b.encode_utf16()));output.push(b'{');for (index,(key,item)) in entries.iter().enumerate(){if index>0{output.push(b',')};output.extend_from_slice(&serde_json::to_vec(key)?);output.push(b':');encode(item,output)?};output.push(b'}');},
 };Ok(())
}
#[cfg(test)]mod tests{use super::*;
 #[test]fn supplementary_keys_use_utf16_order(){let v=serde_json::json!({"\u{e000}":2,"😀":1,"a":"<>&/\n"});assert_eq!(String::from_utf8(to_vec(&v).unwrap()).unwrap(),"{\"a\":\"<>&/\\n\",\"😀\":1,\"\u{e000}\":2}");}
 #[test]fn rejects_rounding_and_unsafe_integers(){assert!(to_vec(&serde_json::json!(1.5)).is_err());assert!(to_vec(&serde_json::json!(9007199254740992u64)).is_err());}
}
