use reqwest::Method;
use reqwest::header::HeaderMap;
use serde_json::{Map, Value, json};
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::ptr;
use std::time::Duration;

// Structure that contains the result of the HTTP request
#[repr(C)]
pub struct FetchResult {
    pub status: i64,               // HTTP status code of the response
    pub headers_json: *mut c_char, // Body of the response as a C string
    pub body: *mut c_char,         // Body of the response as a C string
    pub error: *mut c_char,        // Error message (if any) as a C string
}

// The main FFI function that handles HTTP requests
#[unsafe(no_mangle)]
pub extern "C" fn rs_fetch(
    url: *const c_char,          // URL of the HTTP request
    method: *const c_char,       // HTTP method, e.g., "GET", "POST"
    headers_json: *const c_char, // Headers as a JSON string
    body: *const c_char,         // Body of the request (if applicable)
    timeout_s: i64,              // Timeout for the request in seconds
) -> FetchResult {
    // Initialize the result with default values
    let mut result = FetchResult {
        status: -1,                    // Default to -1 indicating failure
        headers_json: ptr::null_mut(), // No headers initially
        body: ptr::null_mut(),         // No body initially
        error: ptr::null_mut(),        // No error initially
    };

    // Convert the C string to a Rust string for the URL
    let url = unsafe { CStr::from_ptr(url).to_str().unwrap_or_default() };

    // Convert method string to Rust string
    let method_str = unsafe { CStr::from_ptr(method).to_str().unwrap_or("GET") };

    // Parse the HTTP method
    let method: Method = match method_str.parse() {
        Ok(m) => m,
        Err(_) => {
            // Return an error if the HTTP method is invalid
            result.error = CString::new(format!("Invalid HTTP method: {}", method_str))
                .unwrap()
                .into_raw();
            return result;
        }
    };

    // Handle the timeout (default to 5 seconds if not provided)
    let timeout_secs = timeout_s;
    let client = reqwest::blocking::Client::builder()
        .timeout(Duration::from_secs(timeout_secs as u64))
        .build()
        .unwrap();

    // Build the request
    let mut request = client.request(method, url);

    // Add headers if provided
    if !headers_json.is_null() {
        let headers_str = unsafe { CStr::from_ptr(headers_json).to_str().unwrap_or("{}") };
        match serde_json::from_str::<serde_json::Value>(headers_str) {
            Ok(headers) => {
                // If the headers are valid JSON, iterate and add them to the request
                if let Some(headers_obj) = headers.as_object() {
                    for (key, value) in headers_obj {
                        request = request.header(key, value.as_str().unwrap_or_default());
                    }
                }
            }
            Err(_) => {
                // Return an error if the headers format is invalid
                result.error = CString::new("Invalid headers format").unwrap().into_raw();
                return result;
            }
        }
    }

    // Add body to the request if specified
    if !body.is_null() {
        let body_str = unsafe { CStr::from_ptr(body).to_str().unwrap_or("") };
        request = request.body(body_str.to_string());
    }

    // Send the request and handle the response
    match request.send() {
        Ok(resp) => {
            // On success, store the status code
            result.status = resp.status().as_u16() as i64;
            result.headers_json = CString::new(format!("{}", header_map_to_json(resp.headers())))
                .unwrap()
                .into_raw();
            // Read the body of the response
            match resp.text() {
                Ok(body) => {
                    result.body = CString::new(body).unwrap().into_raw();
                }
                Err(e) => {
                    // If reading the body fails, store the error
                    result.error = CString::new(format!("Failed to read body: {}", e))
                        .unwrap()
                        .into_raw();
                }
            }
        }
        Err(e) => {
            // If the request fails, store the error
            result.error = CString::new(format!("Request failed: {}", e))
                .unwrap()
                .into_raw();
        }
    }
    // Return the result containing status, body, and any error
    result
}

#[unsafe(no_mangle)]
pub extern "C" fn rs_free_fetch_result(res: FetchResult) {
    unsafe {
        if !res.headers_json.is_null() {
            let _ = CString::from_raw(res.headers_json);
        }
        if !res.body.is_null() {
            let _ = CString::from_raw(res.body);
        }
        if !res.error.is_null() {
            let _ = CString::from_raw(res.error);
        }
    }
}

fn header_map_to_json(headers: &HeaderMap) -> Value {
    let mut map = Map::new();

    for (key, value) in headers.iter() {
        if let Ok(val_str) = value.to_str() {
            map.insert(key.to_string(), json!(val_str));
        }
    }

    Value::Object(map)
}
