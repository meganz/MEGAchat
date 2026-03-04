pipeline {
    agent { label 'linux && amd64 && android' }
    options {
        buildDiscarder(logRotator(numToKeepStr: '135', daysToKeepStr: '21'))
        gitLabConnection('GitLabConnectionJenkins')
    }
    environment {
        APIURL_TO_TEST = "https://g.api.mega.co.nz/"
        ANDROID_BRANCH = "develop"
        SDK_BRANCH = "develop"
        MEGACHAT_BRANCH = "develop"
    }
    stages {
        stage('Checkout SDK MEGAchat and Android'){
            steps {
                deleteDir()
                checkout([
                    $class: 'GitSCM',
                    branches: [[name: "origin/${env.ANDROID_BRANCH}"]],
                    userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:mobile/android/android.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                    extensions: [
                        [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                        ]
                ])
                dir("sdk/src/main/jni/megachat/sdk"){
                    checkout([
                        $class: 'GitSCM',
                        branches: [[name: "origin/${env.MEGACHAT_BRANCH}"]],
                        userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:megachat/MEGAchat.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                        extensions: [
                            [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"],
                        ]
                    ])
                    script{
                        megachat_sources_workspace = WORKSPACE
                    }
                }
                dir('sdk/src/main/jni/mega/sdk'){
                    sh "echo Cloning SDK branch \"${env.SDK_BRANCH}\""
                    checkout([
                        $class: 'GitSCM',
                        branches: [[name: "origin/${env.SDK_BRANCH}"]],
                            userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:sdk/sdk.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                            extensions: [
                                [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                            ]
                    ])
                    script{
                        sdk_sources_workspace = WORKSPACE
                    }
                }
                script{
                    android_sources_workspace = WORKSPACE
                    sdk_sources_workspace = "${megachat_sources_workspace}/third-party/mega"
                }
            }
        }
        stage('Build MEGACHAT SDK and Android App'){
            environment{
                BUILD_ARCHS = "arm64-v8a"
                ANDROID_HOME = "/home/jenkins/android-cmdlinetools/"
                ANDROID_NDK_HOME ="/home/jenkins/android-ndk/"
                NDK_ROOT = "/home/jenkins/android-ndk/"
                VCPKG_ROOT = "/opt/vcpkg"
                DEFAULT_GOOGLE_MAPS_API_PATH = "/home/jenkins/android-default_google_maps_api"
                USE_PREBUILT_SDK = false
                ARTIFACTORY_BASE_URL = "${env.REPOSITORY_URL}"
            }
            steps{
                dir("sdk/src/main/jni"){
                    script{
                        env.PATH="${env.PATH}:${env.ANDROID_HOME}/cmdline-tools/tools/bin/"
                    }
                    sh """
                        sed -i 's#JOBS=.*#JOBS=1#' cmake.sh
                        bash ./cmake.sh
                    """
                }
                sh "cp -r ${DEFAULT_GOOGLE_MAPS_API_PATH}/* app/src/"
                script {
                    withCredentials([
                            string(credentialsId: 'ARTIFACTORY_USER', variable: 'ARTIFACTORY_USER'),
                            string(credentialsId: 'ARTIFACTORY_ACCESS_TOKEN', variable: 'ARTIFACTORY_ACCESS_TOKEN'),
                    ]){
                        sh "./gradlew --no-daemon --max-workers=1 assembleGms"
                    }
                }
            }
        }
    }
    post {
        always {
            deleteDir()
        }
    }
}
// vim: syntax=groovy tabstop=4 shiftwidth=4
